use anyhow::{anyhow, Context, Result};
use nalgebra as na;
use opencv::{
    calib3d,
    core::{self, AlgorithmHint, Point, Point2f, Point3f, Scalar, Vector},
    highgui, imgproc,
    objdetect,
    prelude::*,
    videoio,
};
use serde::Deserialize;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Deserialize)]
struct CameraYaml {
    camera_matrix: Vec<Vec<f64>>,
    dist_coeffs: Vec<f64>,
}

fn load_camera_params(yaml_path: &Path) -> Result<(Mat, Mat)> {
    let text = fs::read_to_string(yaml_path)
        .with_context(|| format!("Failed to read {}", yaml_path.display()))?;
    let parsed: CameraYaml = serde_yaml::from_str(&text)
        .with_context(|| format!("Failed to parse {}", yaml_path.display()))?;

    if parsed.camera_matrix.len() != 3 || parsed.camera_matrix.iter().any(|row| row.len() != 3) {
        return Err(anyhow!("camera_matrix must be 3x3"));
    }

    let mut k = Mat::zeros(3, 3, core::CV_64F)?.to_mat()?;
    for r in 0..3 {
        for c in 0..3 {
            *k.at_2d_mut::<f64>(r as i32, c as i32)? = parsed.camera_matrix[r][c];
        }
    }

    let mut dist = Mat::zeros(parsed.dist_coeffs.len() as i32, 1, core::CV_64F)?.to_mat()?;
    for (i, coeff) in parsed.dist_coeffs.iter().enumerate() {
        *dist.at_2d_mut::<f64>(i as i32, 0)? = *coeff;
    }

    Ok((k, dist))
}

fn locate_camera_yaml() -> Option<PathBuf> {
    let candidates = [PathBuf::from("src/camera.yaml"), PathBuf::from("../src/camera.yaml")];
    candidates.into_iter().find(|p| p.exists())
}

fn euler_from_rotation_matrix(r: &Mat) -> Result<(f64, f64, f64)> {
    let mut rv = [0.0_f64; 9];
    for row in 0..3 {
        for col in 0..3 {
            rv[row * 3 + col] = *r.at_2d::<f64>(row as i32, col as i32)?;
        }
    }
    let rm = na::Matrix3::from_row_slice(&rv);

    let sy = (rm[(0, 0)] * rm[(0, 0)] + rm[(2, 0)] * rm[(2, 0)]).sqrt();
    let singular = sy < 1e-6;

    let (pitch, yaw, roll) = if !singular {
        (
            rm[(2, 1)].atan2(rm[(2, 2)]),
            (-rm[(2, 0)]).atan2(sy),
            rm[(1, 0)].atan2(rm[(0, 0)]),
        )
    } else {
        ((-rm[(1, 2)]).atan2(rm[(1, 1)]), (-rm[(2, 0)]).atan2(sy), 0.0)
    };

    Ok((pitch.to_degrees(), yaw.to_degrees(), roll.to_degrees()))
}

fn draw_tag_box(frame: &mut Mat, corners: &Vector<Point2f>) -> Result<()> {
    let mut pts = Vector::<Point>::new();
    for c in corners {
        pts.push(Point::new(c.x.round() as i32, c.y.round() as i32));
    }
    let mut contour = Vector::<Vector<Point>>::new();
    contour.push(pts);
    imgproc::polylines(
        frame,
        &contour,
        true,
        Scalar::new(0.0, 255.0, 0.0, 0.0),
        2,
        imgproc::LINE_8,
        0,
    )?;
    Ok(())
}

fn draw_axes(frame: &mut Mat, k: &Mat, dist: &Mat, rvec: &Mat, tvec: &Mat, axis_len: f64) -> Result<()> {
    let mut axis_3d = Vector::<Point3f>::new();
    axis_3d.push(Point3f::new(0.0, 0.0, 0.0));
    axis_3d.push(Point3f::new(axis_len as f32, 0.0, 0.0));
    axis_3d.push(Point3f::new(0.0, axis_len as f32, 0.0));
    axis_3d.push(Point3f::new(0.0, 0.0, axis_len as f32));

    let mut imgpts = Vector::<Point2f>::new();
    calib3d::project_points_def(&axis_3d, rvec, tvec, k, dist, &mut imgpts)?;

    if imgpts.len() != 4 {
        return Ok(());
    }

    let p0 = imgpts.get(0)?;
    let p1 = imgpts.get(1)?;
    let p2 = imgpts.get(2)?;
    let p3 = imgpts.get(3)?;

    let origin = Point::new(p0.x.round() as i32, p0.y.round() as i32);
    imgproc::line(
        frame,
        origin,
        Point::new(p1.x.round() as i32, p1.y.round() as i32),
        Scalar::new(0.0, 0.0, 255.0, 0.0),
        2,
        imgproc::LINE_8,
        0,
    )?;
    imgproc::line(
        frame,
        origin,
        Point::new(p2.x.round() as i32, p2.y.round() as i32),
        Scalar::new(0.0, 255.0, 0.0, 0.0),
        2,
        imgproc::LINE_8,
        0,
    )?;
    imgproc::line(
        frame,
        origin,
        Point::new(p3.x.round() as i32, p3.y.round() as i32),
        Scalar::new(255.0, 0.0, 0.0, 0.0),
        2,
        imgproc::LINE_8,
        0,
    )?;

    Ok(())
}

fn open_camera_with_fallback(primary_index: i32) -> Result<(videoio::VideoCapture, i32)> {
    let candidates = [primary_index, 1];
    for index in candidates {
        let cap = videoio::VideoCapture::new(index, videoio::CAP_ANY)?;
        if cap.is_opened()? {
            return Ok((cap, index));
        }
    }

    Err(anyhow!(
        "Could not open webcam at indices {} or 1. Check camera permissions.",
        primary_index
    ))
}

fn main() -> Result<()> {
    let tag_size_m: f64 = 0.10;
    let cam_index = 0;

    let dictionary =
        objdetect::get_predefined_dictionary(objdetect::PredefinedDictionaryType::DICT_APRILTAG_36h11)?;
    let detector_params = objdetect::DetectorParameters::default()?;
    let refine_params = objdetect::RefineParameters::new_def()?;
    let aruco_detector = objdetect::ArucoDetector::new(&dictionary, &detector_params, refine_params)?;

    let (mut cap, selected_cam_index) = open_camera_with_fallback(cam_index)?;

    let camera_yaml = locate_camera_yaml();
    let use_calibrated = camera_yaml.is_some();

    let half = (tag_size_m / 2.0) as f32;
    let mut obj_pts = Vector::<Point3f>::new();
    obj_pts.push(Point3f::new(-half, -half, 0.0));
    obj_pts.push(Point3f::new(half, -half, 0.0));
    obj_pts.push(Point3f::new(half, half, 0.0));
    obj_pts.push(Point3f::new(-half, half, 0.0));

    let mut k: Option<Mat> = None;
    let mut dist: Option<Mat> = None;

    println!("Press 'q' to quit.");
    println!("Using camera index: {}", selected_cam_index);
    if let Some(path) = &camera_yaml {
        println!("Calibration file found: true ({})", path.display());
    } else {
        println!("Calibration file found: false");
    }

    highgui::named_window("AprilTag_PoseDetector", highgui::WINDOW_AUTOSIZE)?;

    loop {
        let mut frame = Mat::default();
        cap.read(&mut frame)?;
        if frame.empty() {
            break;
        }

        if k.is_none() {
            let sz = frame.size()?;
            let (new_k, new_dist) = if let Some(path) = &camera_yaml {
                load_camera_params(path)?
            } else {
                let w = sz.width as f64;
                let h = sz.height as f64;
                let fx = 0.9 * w;
                let fy = 0.9 * w;
                let cx = w / 2.0;
                let cy = h / 2.0;

                let mut k_guess = Mat::zeros(3, 3, core::CV_64F)?.to_mat()?;
                *k_guess.at_2d_mut::<f64>(0, 0)? = fx;
                *k_guess.at_2d_mut::<f64>(1, 1)? = fy;
                *k_guess.at_2d_mut::<f64>(0, 2)? = cx;
                *k_guess.at_2d_mut::<f64>(1, 2)? = cy;
                *k_guess.at_2d_mut::<f64>(2, 2)? = 1.0;

                let dist_zero = Mat::zeros(5, 1, core::CV_64F)?.to_mat()?;
                (k_guess, dist_zero)
            };
            k = Some(new_k);
            dist = Some(new_dist);
        }

        let k_ref = k.as_ref().unwrap();
        let dist_ref = dist.as_ref().unwrap();

        let mut gray = Mat::default();
        imgproc::cvt_color(
            &frame,
            &mut gray,
            imgproc::COLOR_BGR2GRAY,
            0,
            AlgorithmHint::ALGO_HINT_DEFAULT,
        )?;

        let mut corners = Vector::<Vector<Point2f>>::new();
        let mut ids = Mat::default();
        aruco_detector.detect_markers_def(&gray, &mut corners, &mut ids)?;

        let header_1 = "AprilTag Pose Detector (OpenCV)";
        let header_2 = format!("Tag size: {:.3} m | Family: APRILTAG_36h11", tag_size_m);
        let header_3 = if use_calibrated {
            "Calibrated: YES".to_string()
        } else {
            "Calibrated: NO (approx intrinsics)".to_string()
        };

        for (i, text) in [header_1.to_string(), header_2, header_3].iter().enumerate() {
            imgproc::put_text(
                &mut frame,
                text,
                Point::new(10, 25 + (i as i32) * 20),
                imgproc::FONT_HERSHEY_SIMPLEX,
                0.55,
                Scalar::new(255.0, 255.0, 255.0, 0.0),
                2,
                imgproc::LINE_8,
                false,
            )?;
        }

        if ids.rows() > 0 && !corners.is_empty() {
            let mut best_i = 0;
            let mut best_area = -1.0_f64;

            for i in 0..corners.len() {
                let c = corners.get(i)?;
                let area = imgproc::contour_area(&c, false)?.abs();
                if area > best_area {
                    best_area = area;
                    best_i = i;
                }
            }

            let best_corners = corners.get(best_i)?;
            let tag_id = *ids.at_2d::<i32>(best_i as i32, 0)?;

            draw_tag_box(&mut frame, &best_corners)?;

            let mut rvec = Mat::default();
            let mut tvec = Mat::default();
            let ok = calib3d::solve_pnp(
                &obj_pts,
                &best_corners,
                k_ref,
                dist_ref,
                &mut rvec,
                &mut tvec,
                false,
                calib3d::SOLVEPNP_ITERATIVE,
            )?;

            if ok {
                let tx = *tvec.at_2d::<f64>(0, 0)?;
                let ty = *tvec.at_2d::<f64>(1, 0)?;
                let tz = *tvec.at_2d::<f64>(2, 0)?;
                let distance = (tx * tx + ty * ty + tz * tz).sqrt();

                let mut rmat = Mat::default();
                calib3d::rodrigues(&rvec, &mut rmat, &mut Mat::default())?;
                let (pitch_deg, yaw_deg, roll_deg) = euler_from_rotation_matrix(&rmat)?;

                draw_axes(&mut frame, k_ref, dist_ref, &rvec, &tvec, 0.05)?;

                let mut mean_x = 0.0_f32;
                let mut mean_y = 0.0_f32;
                for i in 0..best_corners.len() {
                    let p = best_corners.get(i)?;
                    mean_x += p.x;
                    mean_y += p.y;
                }
                mean_x /= best_corners.len() as f32;
                mean_y /= best_corners.len() as f32;

                imgproc::circle(
                    &mut frame,
                    Point::new(mean_x.round() as i32, mean_y.round() as i32),
                    4,
                    Scalar::new(0.0, 255.0, 255.0, 0.0),
                    -1,
                    imgproc::LINE_8,
                    0,
                )?;

                let info_lines = [
                    format!("ID: {}", tag_id),
                    format!("Distance to center: {:.3} m (z={:.3} m)", distance, tz),
                    format!(
                        "Rotation (deg): X={:+.1}  Y={:+.1}  Z={:+.1}",
                        pitch_deg, yaw_deg, roll_deg
                    ),
                ];

                for (i, line) in info_lines.iter().enumerate() {
                    imgproc::put_text(
                        &mut frame,
                        line,
                        Point::new(10, 120 + (i as i32) * 24),
                        imgproc::FONT_HERSHEY_SIMPLEX,
                        0.6,
                        Scalar::new(0.0, 255.0, 0.0, 0.0),
                        2,
                        imgproc::LINE_8,
                        false,
                    )?;
                }
            } else {
                imgproc::put_text(
                    &mut frame,
                    "solvePnP failed",
                    Point::new(10, 130),
                    imgproc::FONT_HERSHEY_SIMPLEX,
                    0.7,
                    Scalar::new(0.0, 0.0, 255.0, 0.0),
                    2,
                    imgproc::LINE_8,
                    false,
                )?;
            }
        } else {
            imgproc::put_text(
                &mut frame,
                "No AprilTag detected",
                Point::new(10, 130),
                imgproc::FONT_HERSHEY_SIMPLEX,
                0.7,
                Scalar::new(0.0, 0.0, 255.0, 0.0),
                2,
                imgproc::LINE_8,
                false,
            )?;
        }

        highgui::imshow("AprilTag_PoseDetector", &frame)?;
        let key = highgui::wait_key(1)?;
        if key == 'q' as i32 {
            break;
        }
    }

    cap.release()?;
    highgui::destroy_all_windows()?;
    Ok(())
}

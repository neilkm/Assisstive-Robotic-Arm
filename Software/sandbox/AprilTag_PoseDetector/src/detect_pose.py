import cv2
import numpy as np
import yaml
from pathlib import Path
import math

def load_camera_params(yaml_path: str):
    """
    Loads camera intrinsics K and distortion coeffs dist from a YAML file.
    Expected format:
      camera_matrix: [[fx, 0, cx], [0, fy, cy], [0, 0, 1]]
      dist_coeffs: [k1, k2, p1, p2, k3]  # length can vary (4/5/8)
    """
    with open(yaml_path, "r") as f:
        data = yaml.safe_load(f)
    K = np.array(data["camera_matrix"], dtype=np.float64)
    dist = np.array(data["dist_coeffs"], dtype=np.float64).reshape(-1, 1)
    return K, dist

def euler_from_rotation_matrix(R: np.ndarray):
    """
    Convert rotation matrix to Euler angles in degrees.

    Output labels:
      X = pitch (about camera +X)
      Y = yaw   (about camera +Y)
      Z = roll  (about camera +Z)

    Camera frame (OpenCV): +x right, +y down, +z forward
    """
    sy = math.sqrt(R[0, 0]*R[0, 0] + R[2, 0]*R[2, 0])
    singular = sy < 1e-6

    if not singular:
        pitch = math.atan2(R[2, 1], R[2, 2])     # about x
        yaw   = math.atan2(-R[2, 0], sy)         # about y
        roll  = math.atan2(R[1, 0], R[0, 0])     # about z
    else:
        pitch = math.atan2(-R[1, 2], R[1, 1])
        yaw   = math.atan2(-R[2, 0], sy)
        roll  = 0.0

    return (math.degrees(pitch), math.degrees(yaw), math.degrees(roll))

def draw_tag_box(frame, corners, color=(0, 255, 0), thickness=2):
    pts = corners.astype(int).reshape(-1, 1, 2)
    cv2.polylines(frame, [pts], isClosed=True, color=color, thickness=thickness)

def draw_axes(frame, K, dist, rvec, tvec, axis_len=0.05):
    """
    Draw 3D axes from the tag origin:
      X=red, Y=green, Z=blue
    axis_len in meters.
    """
    axis_3d = np.array([
        [0, 0, 0],
        [axis_len, 0, 0],
        [0, axis_len, 0],
        [0, 0, axis_len]
    ], dtype=np.float64)

    imgpts, _ = cv2.projectPoints(axis_3d, rvec, tvec, K, dist)
    imgpts = imgpts.reshape(-1, 2).astype(int)

    origin = tuple(imgpts[0])
    cv2.line(frame, origin, tuple(imgpts[1]), (0, 0, 255), 2)   # X red
    cv2.line(frame, origin, tuple(imgpts[2]), (0, 255, 0), 2)   # Y green
    cv2.line(frame, origin, tuple(imgpts[3]), (255, 0, 0), 2)   # Z blue

def main():
    # === CONFIG ===
    tag_size_m = 0.10  # <-- SET THIS to your measured black-square edge length in meters
    cam_index = 0

    # OpenCV AprilTag detector (requires opencv-contrib-python)
    aruco = cv2.aruco
    tag_dict = aruco.getPredefinedDictionary(aruco.DICT_APRILTAG_36h11)
    params = aruco.DetectorParameters()
    detector = aruco.ArucoDetector(tag_dict, params)

    cap = cv2.VideoCapture(cam_index)
    if not cap.isOpened():
        raise RuntimeError("Could not open webcam. Try cam_index=1 or check permissions.")

    # Calibration load (optional)
    camera_yaml = Path(__file__).parent / "camera.yaml"
    use_calibrated = camera_yaml.exists()

    K = None
    dist = None

    # 3D model points for tag corners (meters), tag-centered.
    # OpenCV's detectMarkers returns corners in consistent order around the tag.
    s = tag_size_m
    half = s / 2.0
    obj_pts = np.array([
        [-half, -half, 0],
        [ half, -half, 0],
        [ half,  half, 0],
        [-half,  half, 0],
    ], dtype=np.float64)

    print("Press 'q' to quit.")
    print(f"Calibration file found: {use_calibrated} ({camera_yaml})")

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # Init intrinsics
        if K is None:
            h, w = frame.shape[:2]
            if use_calibrated:
                K, dist = load_camera_params(str(camera_yaml))
            else:
                # Rough guess for demo; for real metric accuracy, run calibration.
                fx = 0.9 * w
                fy = 0.9 * w
                cx = w / 2.0
                cy = h / 2.0
                K = np.array([[fx, 0, cx],
                              [0, fy, cy],
                              [0,  0,  1]], dtype=np.float64)
                dist = np.zeros((5, 1), dtype=np.float64)

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        corners_list, ids, _ = detector.detectMarkers(gray)

        # UI header
        header = [
            "AprilTag Pose Detector (OpenCV)",
            f"Tag size: {tag_size_m:.3f} m | Family: APRILTAG_36h11",
            "Calibrated: YES" if use_calibrated else "Calibrated: NO (approx intrinsics)"
        ]
        y0 = 25
        for i, t in enumerate(header):
            cv2.putText(frame, t, (10, y0 + i*20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255, 255, 255), 2)

        if ids is not None and len(ids) > 0:
            # Pick the largest detection (closest / best for demo)
            best_i = None
            best_area = -1.0
            for i, c in enumerate(corners_list):
                area = abs(cv2.contourArea(c.astype(np.float32)))
                if area > best_area:
                    best_area = area
                    best_i = i

            corners = corners_list[best_i].reshape(4, 2).astype(np.float64)
            tag_id = int(ids[best_i][0])

            draw_tag_box(frame, corners)

            img_pts = corners.reshape(-1, 1, 2)

            # For squares, IPPE can be very good; fall back to iterative if needed.
            # Not all OpenCV builds support all flags equally; iterative is universally available.
            ok, rvec, tvec = cv2.solvePnP(
                obj_pts, img_pts, K, dist,
                flags=cv2.SOLVEPNP_ITERATIVE
            )

            if ok:
                tx, ty, tz = tvec.flatten()
                distance = float(np.linalg.norm(tvec))

                R, _ = cv2.Rodrigues(rvec)
                pitch_deg, yaw_deg, roll_deg = euler_from_rotation_matrix(R)

                # Draw axes (optional but nice)
                draw_axes(frame, K, dist, rvec, tvec, axis_len=0.05)

                # Center point
                center_px = tuple(np.mean(corners, axis=0).astype(int))
                cv2.circle(frame, center_px, 4, (0, 255, 255), -1)

                info = [
                    f"ID: {tag_id}",
                    f"Distance to center: {distance:.3f} m (z={tz:.3f} m)",
                    f"Rotation (deg): X={pitch_deg:+.1f}  Y={yaw_deg:+.1f}  Z={roll_deg:+.1f}"
                ]
                y1 = 120
                for i, line in enumerate(info):
                    cv2.putText(frame, line, (10, y1 + i*24),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
            else:
                cv2.putText(frame, "solvePnP failed", (10, 130),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
        else:
            cv2.putText(frame, "No AprilTag detected", (10, 130),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        cv2.imshow("AprilTag_PoseDetector", frame)
        if (cv2.waitKey(1) & 0xFF) == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()


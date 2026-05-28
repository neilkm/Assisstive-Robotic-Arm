# AprilTag_PoseDetector

Python + OpenCV sandbox app that detects AprilTags from a webcam and computes pose (distance, rotation XYZ). Built for macOS (M2 MacBook Air).

Detects `APRILTAG_36h11` tags. Shows a live video stream with a bounding-box overlay and a display of distance to the tag center plus XYZ rotation angles.

## Layout

```
src/
  detect_pose.py        Main OpenCV pose detector
  camera_calibrate.py   Optional camera calibration (produces src/camera.yaml)
rust_pose_detector/     Rust implementation of the same detector
  src/main.rs
  Cargo.toml
requirements.txt
```

## Build and run

```bash
Software/arm.sh sandbox build apriltag-detector   # create venv + install deps
Software/arm.sh sandbox run   apriltag-detector   # run detector
```

Allow camera access for Terminal when prompted. Press `q` to quit.

## Calibration (optional)

Camera calibration improves pose accuracy. Run before the detector:

```bash
Software/arm.sh sandbox run apriltag-detector --calibrate
```

This produces `src/camera.yaml`, which the detector uses automatically.

## Rust detector

```bash
cd Software/sandbox/AprilTag_PoseDetector/rust_pose_detector
cargo run
```

## Generating printable AprilTag PDFs

```bash
Software/arm.sh tools run apriltag-pdf
```

Tags should be printed at exactly 10 cm — that is the size the detector expects.

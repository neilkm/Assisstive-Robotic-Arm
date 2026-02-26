# AprilTag_PoseDetector
Coding demo for OpenCV proof of concept for 2026 Robotics Eng. Capstone project.

Goal is to recognize the distance, rotation, and angle of a known size AprilTag placed
in front of the camera. 

App should show:

- a video stream with a box overlay around the AprilTag

- a live updating display of:

	- distance to the center of the AprilTag

  	- rotation angle in X Y Z

# Getting Started

this project was built to run on M2 MacBookAir

## 1. Install Dependencies

use bash script for installing dependencies and running project:

$ chmod +x setup.sh

$ ./setup.sh


make sure to allow shell application to access camera. may need to restart terminal.

press 'q' to quit app once dependencies are installed and confirmed to be working.

## 2. Generate pdfs of AprilTags to print out

tags taken from:

https://github.com/AprilRobotics/apriltag-imgs.git

use bash script for cloning the repo and selecting which tag id to print

$ chmod +x ./tools/generate_tag_pdf.sh 

$ ./tools/generate_tag_pdf.sh

a new directory in root now contains a printable pdf of the AprilTag you chose
sized to 10cm exactly which is what the code expects

$ cd ./generated_pdfs/ 

## 3. Run code and be amazed.

$ ./setup.sh

## 4. Run the Rust AprilTag detector

The Rust app lives in `rust_pose_detector/` and opens your webcam to detect `APRILTAG_36h11` tags.

Optional but recommended: create camera calibration first so pose estimates are more accurate:

```bash
./setup.sh --calibrate
```

This creates `src/camera.yaml`, which the Rust app will automatically use.

From `Software/AprilTag_PoseDetector`:

```bash
cd rust_pose_detector
cargo run
```

Or from repo root:

```bash
cargo run --manifest-path Software/AprilTag_PoseDetector/rust_pose_detector/Cargo.toml
```

Notes:
- Allow camera permission for Terminal when prompted.
- Press `q` in the detector window to quit.

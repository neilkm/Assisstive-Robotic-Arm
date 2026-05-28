# apriltag-ik-pose-poc

Qt/QML + C++ sandbox that validates the AprilTag pose pipeline for the assistive robot arm.

Commands a target XYZ point, solves inverse kinematics for joint angles, visualises the arm in 3D (blue joint dots, yellow link lines), overlays simulated AprilTag centres (purple dots), then compares the FK end-effector position against the AprilTag-derived end-effector estimate. When the two match, the pose-estimation math is consistent.

## Build and run

```bash
Software/arm.sh sandbox build apriltag-ik     # CMake build → builds/sandbox/apriltag-ik/
Software/arm.sh sandbox run   apriltag-ik     # Qt/QML GUI
Software/arm.sh sandbox run   apriltag-ik-backend  # CLI smoke test
```

## Layout

```
CMakeLists.txt
configs/
  arm_geometry.csv          Joint DH table + AprilTag placement
qml/
  main.qml
src/
  PoseController.cpp/hpp
  AprilTagEndEffectorEstimator.cpp/hpp
  ArmForwardKinematics.cpp/hpp
  ArmGeometry.cpp/hpp
  kinematics.cpp/hpp
  main.cpp                  CLI backend entry point
  qml_main.cpp              Qt/QML entry point
tests/
  kinematics_test.cpp
  arm_forward_kinematics_test.cpp
  april_tag_end_effector_estimator_test.cpp
```

## GUI pages

- **Pose** — XYZ target input, approach angle, joint5 end-effector rotation, AprilTag visibility toggles, arm/tag visualisation, end-effector comparison errors, `Cube` test button.
- **Config** — editable link lengths and 6-row DH table; `Save` rewrites `configs/arm_geometry.csv` and reloads the backend.

## Configuration

Edit `configs/arm_geometry.csv` to change link lengths, DH parameters, joint limits, and AprilTag local offsets. Replace default proof-of-concept values with measured values from the real arm before using for hardware validation.

## OpenCV integration point

Replace the simulated tag generation with a real camera-backed function:

```cpp
std::vector<arm_pose_estimator::AprilTagStatus> tag_statuses =
    read_tag_statuses_from_camera(...);
```

The coordinate frame must match the robot base frame.

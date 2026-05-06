# AprilTag IK Pose Proof Of Concept

## Original Prompt

```text
i have a robot arm that has apriltags placed on various parts of it, i want to tell the robot arm to to a certain xyz point and i want an inverse kinematics equation to figure out the joint angles to get it there, i want an opencv camera looking at the apriltags on the arm figuring out their positions to calculate the joint angles and end effector position separately.

as a proof of concept. i want an app that shows a 3d coordinate grid with the rotating joints of the robot visualized as blue dots and the distances between the joints as yellow lines irrespective of the width of the actual mechanical design, floating around these yellow lines but tracking them exactly should be purple dots that show the center of the april tags that are stuck onto the robot. when an xyz coordinate is given by the user, inverse kinematics should move the joints accordingly with the april tags following. the actual xyz position of the end effector should be shown to prove that the inverse kinematics was correct. the actual coordinates of all the april tags should be read by another function which should use kinematics to calculate what the position of the end effector must be. comparing the actual position of the end effector with the calculated position of the end effector given the position of the april tags will prove that our pose detection works.

in Software/sandbox i want to make a new project that does that. copy this whole prompt into a readme in that new folder
```

## Purpose

This sandbox is a visual proof of concept for validating an assistive robotic arm pose pipeline:

- Command a target XYZ point.
- Solve inverse kinematics for the joint angles.
- Draw the true kinematic arm as blue joint dots and yellow link lines.
- Draw simulated AprilTag centers as purple dots attached to the arm.
- Reconstruct joint angles and end-effector position from the AprilTag center positions through a separate measurement path.
- Compare the commanded/FK end-effector position against the AprilTag-derived end-effector estimate.

The current project simulates AprilTag detections. It is structured so the simulated tag reader can later be replaced by an OpenCV AprilTag detector that returns real 3D tag-center coordinates.

The kinematics and Qt controller are C++. The operator UI is Qt/QML.

## Project Layout

```text
Software/sandbox/apriltag-ik-pose-poc/
  CMakeLists.txt
  README.md
  build.sh
  run.sh
  configs/
    apriltags.csv
    dh_table.csv
    robot_dimensions.csv
  qml/
    main.qml
    apriltag_ik_pose_qml.qrc
  src/
    PoseController.cpp
    PoseController.hpp
    kinematics.cpp
    kinematics.hpp
    main.cpp
    qml_main.cpp
  tests/
    kinematics_test.cpp
    qml/
      tst_main.qml
```

## How It Works

The app keeps two pose paths separate:

1. **Commanded path**
   - User enters target `X`, `Y`, `Z` in meters.
   - The C++ damped least-squares inverse kinematics solver reads the 6-joint DH table and solves joint angles.
   - Forward kinematics gives the actual end-effector position for those angles.

2. **AprilTag-derived path**
   - Tag centers are generated from the true joint state in this proof of concept.
   - A separate C++ estimator uses the observed tag centers and the same robot geometry to recover joint angles.
   - Forward kinematics on the recovered angles gives the tag-derived end-effector estimate.

If the tag-derived end-effector estimate matches the actual FK end-effector position, the pose-estimation math path is consistent. In real camera usage, the simulated tag-center function should be replaced by an OpenCV detector that reports calibrated 3D tag centers.

## Run

```bash
cd Software/sandbox/apriltag-ik-pose-poc
./run.sh
```

Run the non-GUI smoke test:

```bash
./run.sh smoke
```

Build only:

```bash
./build.sh
```

Run all tests with visible output:

```bash
./test.sh
```

Remove generated build and test output:

```bash
./clean.sh
```

Run the C++ backend directly:

```bash
APRILTAG_IK_POSE_POC_ROOT="$PWD" ./build/apriltag_ik_pose_backend --target 0.36 0.11 0.24
APRILTAG_IK_POSE_POC_ROOT="$PWD" ./build/apriltag_ik_pose_backend --zero
APRILTAG_IK_POSE_POC_ROOT="$PWD" ./build/apriltag_ik_pose_backend --angles 0 0 0 0 0 0
APRILTAG_IK_POSE_POC_ROOT="$PWD" ./build/apriltag_ik_pose_backend --trajectory 0 0 0 0 0 0 5 0 0 0 0 0 5 5
```

The Qt/QML GUI animates joint motion at `5 deg/s` through the C++ `PoseController`. Click-dragging the 3D view rotates only around the Z axis.

The GUI has two pages:

- `Pose`: XYZ target input, arm visualization, AprilTag visualization, end-effector comparison, and a `Cube` test button.
- `Config`: editable distance constants and editable six-row DH table. `Save` rewrites the CSV files and reloads the C++ backend.

The `Cube` test commands the end effector through eight target points arranged as the vertices of a cube. The cube bottom face lies on the joint0 plane, the bottom face is centered around joint0, and each side is 12 units long. After the eighth point, the test returns the robot to zero joint angles and stops.

## Configuration

Edit `configs/robot_dimensions.csv` to change the symbolic dimensions:

```text
L0 L1 L2 L3 L4 L5 W0 W1 W2 G0
```

The default proof-of-concept values are stored in `configs/robot_dimensions.csv`.


Edit `configs/dh_table.csv` to change the six robot joints. The table is:

```text
name,a_m,alpha_rad,d_m,theta_offset_rad,initial_deg,min_deg,max_deg
```

The backend currently requires exactly 6 rows after the header.

The current DH rows were derived from the requested zero state:

```text
joint0: a=L0, alpha=-pi/2, d=0,  theta_offset=0
joint1: a=L1, alpha=0,     d=0,  theta_offset=-pi/2
joint2: a=L2, alpha=-pi/2, d=0,  theta_offset=0
joint3: a=L3, alpha=pi/2,  d=0,  theta_offset=0
joint4: a=L4, alpha=-pi/2, d=0,  theta_offset=0
joint5: a=0,  alpha=0,     d=L5, theta_offset=0
```

With all joint angles at 0 degrees, this places:

```text
joint0 = [0, 0, 0]
joint1 = [L0, 0, 0]
joint2 = [L0, 0, L1]
joint3 = [L0, 0, L1 + L2]
joint4 = [L0, 0, L1 + L2 + L3]
joint5 = [L0, 0, L1 + L2 + L3 + L4]
end effector = [L0 + L5, 0, L1 + L2 + L3 + L4]
```

Edit `configs/apriltags.csv` to change AprilTag placement:

```text
id,attached_after_joint,local_x_m,local_y_m,local_z_m
```

`attached_after_joint` is zero-based. For example, `2` means the tag local coordinate is attached after joint 3's DH transform.

The current tag rows encode the requested zero-state placement:

```text
apriltag0/1: centered on the joint1-joint2 link midpoint, separated along X by W0
apriltag2/3: centered on the joint3-joint4 link midpoint, separated along Y by W1
apriltag4/5: centered on joint5, separated along Y by W2
apriltag6/7: centered on the end effector, separated along Y by G0
```

The default geometry is only a proof-of-concept model. Replace the link lengths, offsets, limits, and AprilTag local coordinates with measured values from the actual arm before using this for hardware validation.

## OpenCV Integration Point

Replace the simulated tag-center generation in `src/main.cpp` with a camera-backed OpenCV function that returns tag centers in the same order as `configs/apriltags.csv`:

```cpp
std::vector<arm::Vec3> observed_tags = read_tags_from_camera(...);
```

The coordinate frame must match the robot base frame. If the camera frame is different, add a calibrated camera-to-base transform before passing tag centers into `estimate_pose_from_tags()`.

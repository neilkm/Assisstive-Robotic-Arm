# robot-arm-3d-sim

Python sandbox for simulating the robot arm kinematics. Config-driven 6-DOF arm with forward kinematics, damped least-squares inverse kinematics, joint limits, and a GUI that accepts XYZ targets or direct joint-angle commands.

## Build and run

```bash
Software/arm.sh sandbox build robot-sim    # create venv + install deps
Software/arm.sh sandbox run   robot-sim    # launch GUI
Software/arm.sh sandbox run   robot-sim cli  # CLI FK output only
```

## Layout

```
configs/
  robot_arm.yaml    Joint geometry, DH parameters, joint limits, IK tuning
src/
  kinematics.py     FK, Jacobian, damped-least-squares IK, trajectory generation
  xyz_gui.py        GUI: XYZ input, joint angle fields, 3D arm view
```

## Configuration

Edit `configs/robot_arm.yaml` to change DOF, link lengths, joint limits, and IK tuning parameters.

## GUI controls

- **Move To XYZ** — solve IK for a target position and animate the arm.
- **Apply Joint Angles** — set joint angles directly.
- **Rx / Ry / Rz sliders** — rotate the 3D view.
- **Reset View** — restore default camera orientation.

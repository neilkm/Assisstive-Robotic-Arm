# Robot Arm 3D Simulation Sandbox

This sandbox now includes a configurable kinematics implementation:
- config-driven DOF and joint geometry,
- config-driven joint limits,
- forward kinematics (FK),
- damped least squares inverse kinematics (IK),
- range trajectory generation for simulation tests,
- a GUI that accepts XYZ input or a single-joint angle command.

## Implemented Files

```text
Software/sandbox/robot-arm-3d-sim/
  README.md
  configs/
    robot_arm.yaml
  src/
    kinematics.py
    xyz_gui.py
  models/
    meshes/
  tests/
```

## User-Adjustable Configuration

Edit `configs/robot_arm.yaml`.

- DOF: number of joints equals the length of `robot.joints`.
- Joint lengths/geometry: `a_m`, `d_m`, `alpha_rad`, `theta_offset_rad` per joint.
- Joint rotation axis label: `rotation_axis_local`.
- Joint starting angle: `initial_deg`.
- Joint hard limits: `min_deg`, `max_deg`.
- IK tuning: `simulation.ik.max_iters`, `damping`, `tolerance_m`.
- Default setup now starts with 6 DOF (`joint_1` ... `joint_6`).

## Detailed Equations (Implemented)

### 1) DH Transform Per Joint

For each joint `i` (revolute), with:
- `a_i = a_m`
- `alpha_i = alpha_rad`
- `d_i = d_m`
- `theta_i = deg2rad(q_i_deg) + theta_offset_rad`

the homogeneous transform is:

```text
A_i =
[ cos(theta_i)  -sin(theta_i)cos(alpha_i)   sin(theta_i)sin(alpha_i)   a_i cos(theta_i) ]
[ sin(theta_i)   cos(theta_i)cos(alpha_i)  -cos(theta_i)sin(alpha_i)   a_i sin(theta_i) ]
[      0                 sin(alpha_i)               cos(alpha_i)              d_i         ]
[      0                      0                          0                     1           ]
```

### 2) Forward Kinematics

Base-to-end-effector transform:

```text
T_0N(q) = A_1(q_1) A_2(q_2) ... A_N(q_N)
```

End-effector position used by IK:

```text
p(q) = [T_0N(0,3), T_0N(1,3), T_0N(2,3)]^T
```

### 3) Numerical Jacobian (Position Only)

For each joint `j`, using perturbation `eps_deg`:

```text
J(:,j) = ( p(q + eps_deg * e_j) - p(q) ) / deg2rad(eps_deg)
```

This yields `J in R^(3xN)` mapping joint-rate radians to Cartesian linear velocity.

### 4) Damped Least Squares IK Update

Given position error:

```text
e = p_target - p(q)
```

Compute step in radians:

```text
Delta_q_rad = J^T (J J^T + lambda^2 I)^(-1) e
```

Convert and apply:

```text
q_deg <- q_deg + rad2deg(Delta_q_rad)
q_deg <- clamp(q_deg, min_deg, max_deg)
```

Converges when:

```text
||e|| < tolerance_m
```

### 5) Full-Range Trajectory (From Min To Max)

With per-joint config values `q0_i = min_deg`, `q1_i = max_deg`:

```text
q_i(k) = q0_i + (k/(S-1)) (q1_i - q0_i),  k=0..S-1
```

Each step is clamped to `[min_deg, max_deg]`.

## How to Use

1. Edit `configs/robot_arm.yaml` for your arm dimensions and limits.
2. Launch the GUI:
   - `./run.sh`
3. Option A: Enter target `X`, `Y`, `Z` in meters and click `Move To XYZ`.
4. Option B: Edit the displayed per-joint angle fields and click `Apply Joint Angles`.
5. Option C: Adjust view rotation with `Rx`, `Ry`, `Rz` sliders (or click `Reset View`).
6. The arm animates from current joint state to the commanded result.
7. GUI always shows:
   - real end-effector XYZ readout,
   - per-joint angle readouts,
   - red marker for actual end effector,
   - orange joint-range arcs centered on each joint (with current-angle marker).
8. To run only CLI FK output:
   - `./run.sh cli`

## Best Next Step For 3D Simulation

Use this module as the solver backend and connect it to PyBullet:
1. Load URDF in PyBullet.
2. Read `robot_arm.yaml`.
3. Use IK output joint angles to command simulator joints.
4. Compare simulator end-effector pose with `fk()` output to validate model consistency.

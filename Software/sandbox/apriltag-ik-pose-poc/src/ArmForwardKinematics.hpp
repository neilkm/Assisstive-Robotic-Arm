#pragma once

#include "ArmGeometry.hpp"

#include <array>
#include <vector>

namespace arm_fk {

/** Homogeneous transform for points represented as column vectors. */
struct Transform {
  double m[4][4] = {};
};

/** Full generated pose state for a specific joint angle vector. */
struct GeneratedRobotState {
  std::vector<double> joint_angles_deg;
  std::vector<arm_geometry::Pose3> joint_poses;
  std::vector<arm_geometry::Pose3> april_tag_poses;
  arm_geometry::Pose3 end_effector_pose;
};

/** Compute all joint, AprilTag, and end-effector poses from joint angles. */
GeneratedRobotState GenerateRobotState(
    const arm_geometry::ArmGeometry& geometry,
    const std::vector<double>& joint_angles_deg);

/** Solve position-only IK with damped least squares. */
bool SolveIkPositionOnly(const arm_geometry::ArmGeometry& geometry,
                         arm_geometry::Vec3 target_xyz_m,
                         const std::vector<double>& initial_angles_deg,
                         int max_iters, double damping,
                         double tolerance_m,
                         std::vector<double>* solved_angles_deg,
                         double* final_error_m);

/** Solve position IK while holding any true entries in fixed_joints fixed. */
bool SolveIkPositionOnlyWithFixedJoints(
    const arm_geometry::ArmGeometry& geometry,
    arm_geometry::Vec3 target_xyz_m,
    const std::vector<double>& initial_angles_deg,
    const std::vector<bool>& fixed_joints,
    int max_iters, double damping,
    double tolerance_m,
    std::vector<double>* solved_angles_deg,
    double* final_error_m);

/** Distance between two Cartesian points. */
double Distance(arm_geometry::Vec3 a, arm_geometry::Vec3 b);

/** Convert degrees to radians for tests and UI code. */
double DegToRad(double deg);

/** Convert radians to degrees for tests and UI code. */
double RadToDeg(double rad);

Transform IdentityTransform();
Transform Multiply(Transform a, Transform b);
Transform PoseToTransform(arm_geometry::Pose3 pose);
Transform InverseRigidTransform(Transform transform);
arm_geometry::Pose3 TransformToPose(Transform transform);

}  // namespace arm_fk

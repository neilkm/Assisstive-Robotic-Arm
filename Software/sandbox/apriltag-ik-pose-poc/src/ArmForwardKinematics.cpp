#include "ArmForwardKinematics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr double kPi = 3.14159265358979323846;

arm_fk::Transform DhTransform(double a_m, double alpha_rad, double d_m,
                              double theta_rad) {
  // Standard Denavit-Hartenberg transform. The original convention is from
  // Denavit and Hartenberg, "A kinematic notation for lower-pair mechanisms
  // based on matrices", Journal of Applied Mechanics, 1955,
  // DOI: https://doi.org/10.1115/1.4011045.
  const double cth = std::cos(theta_rad);
  const double sth = std::sin(theta_rad);
  const double cal = std::cos(alpha_rad);
  const double sal = std::sin(alpha_rad);

  arm_fk::Transform out{};
  out.m[0][0] = cth;
  out.m[0][1] = -sth * cal;
  out.m[0][2] = sth * sal;
  out.m[0][3] = a_m * cth;
  out.m[1][0] = sth;
  out.m[1][1] = cth * cal;
  out.m[1][2] = -cth * sal;
  out.m[1][3] = a_m * sth;
  out.m[2][1] = sal;
  out.m[2][2] = cal;
  out.m[2][3] = d_m;
  out.m[3][3] = 1.0;
  return out;
}

arm_geometry::Vec3 TransformPoint(arm_fk::Transform transform,
                                  arm_geometry::Vec3 point) {
  return {
      transform.m[0][0] * point.x + transform.m[0][1] * point.y +
          transform.m[0][2] * point.z + transform.m[0][3],
      transform.m[1][0] * point.x + transform.m[1][1] * point.y +
          transform.m[1][2] * point.z + transform.m[1][3],
      transform.m[2][0] * point.x + transform.m[2][1] * point.y +
          transform.m[2][2] * point.z + transform.m[2][3],
  };
}

std::vector<arm_fk::Transform> JointTransforms(
    const arm_geometry::ArmGeometry& geometry,
    const std::vector<double>& joint_angles_deg) {
  if (joint_angles_deg.size() != geometry.joints.size()) {
    throw std::runtime_error("Joint angle count does not match geometry");
  }

  std::vector<arm_fk::Transform> transforms;
  transforms.reserve(geometry.joints.size() + 1);
  arm_fk::Transform t = arm_fk::IdentityTransform();
  transforms.push_back(t);
  for (size_t i = 0; i < geometry.joints.size(); ++i) {
    const arm_geometry::JointGeometry& joint = geometry.joints[i];
    const double theta_rad =
        arm_fk::DegToRad(joint_angles_deg[i]) + joint.theta_offset_rad;
    t = arm_fk::Multiply(
        t, DhTransform(joint.a_m, joint.alpha_rad, joint.d_m, theta_rad));
    transforms.push_back(t);
  }
  return transforms;
}

std::vector<std::vector<double>> PositionJacobian(
    const arm_geometry::ArmGeometry& geometry,
    const std::vector<double>& joint_angles_deg) {
  constexpr double kStepDeg = 0.1;
  const arm_geometry::Vec3 p0 =
      arm_fk::GenerateRobotState(geometry, joint_angles_deg)
          .end_effector_pose.position_m;
  std::vector<std::vector<double>> jacobian(
      3, std::vector<double>(joint_angles_deg.size(), 0.0));

  for (size_t i = 0; i < joint_angles_deg.size(); ++i) {
    std::vector<double> stepped = joint_angles_deg;
    stepped[i] += kStepDeg;
    const arm_geometry::Vec3 p1 =
        arm_fk::GenerateRobotState(geometry, stepped)
            .end_effector_pose.position_m;
    const double step_rad = arm_fk::DegToRad(kStepDeg);
    jacobian[0][i] = (p1.x - p0.x) / step_rad;
    jacobian[1][i] = (p1.y - p0.y) / step_rad;
    jacobian[2][i] = (p1.z - p0.z) / step_rad;
  }
  return jacobian;
}

std::vector<double> SolveLinearSystem(std::vector<std::vector<double>> a,
                                      std::vector<double> b) {
  const int n = static_cast<int>(b.size());
  for (int col = 0; col < n; ++col) {
    int pivot = col;
    for (int row = col + 1; row < n; ++row) {
      if (std::fabs(a[row][col]) > std::fabs(a[pivot][col])) {
        pivot = row;
      }
    }
    if (std::fabs(a[pivot][col]) < 1e-12) {
      throw std::runtime_error("Singular linear system in IK solve");
    }
    std::swap(a[col], a[pivot]);
    std::swap(b[col], b[pivot]);

    const double div = a[col][col];
    for (int c = col; c < n; ++c) {
      a[col][c] /= div;
    }
    b[col] /= div;

    for (int row = 0; row < n; ++row) {
      if (row == col) {
        continue;
      }
      const double factor = a[row][col];
      for (int c = col; c < n; ++c) {
        a[row][c] -= factor * a[col][c];
      }
      b[row] -= factor * b[col];
    }
  }
  return b;
}

bool IsFixedJoint(const std::vector<bool>& fixed_joints, size_t index) {
  return index < fixed_joints.size() && fixed_joints[index];
}

bool SolveIkPositionOnlyInternal(
    const arm_geometry::ArmGeometry& geometry,
    arm_geometry::Vec3 target_xyz_m,
    const std::vector<double>& initial_angles_deg,
    const std::vector<bool>& fixed_joints,
    int max_iters,
    double damping,
    double tolerance_m,
    std::vector<double>* solved_angles_deg,
    double* final_error_m) {
  std::vector<double> q =
      arm_geometry::ClampJointAngles(geometry, initial_angles_deg);
  const std::vector<double> fixed_values = q;

  for (int iter = 0; iter < max_iters; ++iter) {
    const arm_geometry::Vec3 current =
        arm_fk::GenerateRobotState(geometry, q).end_effector_pose.position_m;
    const arm_geometry::Vec3 err{target_xyz_m.x - current.x,
                                 target_xyz_m.y - current.y,
                                 target_xyz_m.z - current.z};
    const double err_norm = arm_fk::Distance(target_xyz_m, current);
    if (err_norm <= tolerance_m) {
      *solved_angles_deg = q;
      *final_error_m = err_norm;
      return true;
    }

    const std::vector<std::vector<double>> jacobian =
        PositionJacobian(geometry, q);
    // Damped least squares solves dq = J^T (J J^T + lambda^2 I)^-1 e.
    // The damping term improves behavior near singular configurations; see
    // Wampler, "Manipulator inverse kinematic solutions based on vector
    // formulations and damped least-squares methods", IEEE TSMC, 1986,
    // DOI: https://doi.org/10.1109/TSMC.1986.289285.
    std::vector<std::vector<double>> lhs(3, std::vector<double>(3, 0.0));
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        for (size_t k = 0; k < q.size(); ++k) {
          if (!IsFixedJoint(fixed_joints, k)) {
            lhs[r][c] += jacobian[r][k] * jacobian[c][k];
          }
        }
      }
      lhs[r][r] += damping * damping;
    }

    const std::vector<double> y = SolveLinearSystem(lhs, {err.x, err.y, err.z});
    for (size_t i = 0; i < q.size(); ++i) {
      if (IsFixedJoint(fixed_joints, i)) {
        q[i] = fixed_values[i];
        continue;
      }

      double dq_rad = 0.0;
      for (int r = 0; r < 3; ++r) {
        dq_rad += jacobian[r][i] * y[r];
      }
      q[i] += arm_fk::RadToDeg(dq_rad);
    }
    q = arm_geometry::ClampJointAngles(geometry, q);
    for (size_t i = 0; i < q.size(); ++i) {
      if (IsFixedJoint(fixed_joints, i)) {
        q[i] = fixed_values[i];
      }
    }
  }

  *solved_angles_deg = q;
  *final_error_m =
      arm_fk::Distance(target_xyz_m, arm_fk::GenerateRobotState(geometry, q)
                                         .end_effector_pose.position_m);
  return false;
}

}  // namespace

namespace arm_fk {

double DegToRad(double deg) { return deg * kPi / 180.0; }

double RadToDeg(double rad) { return rad * 180.0 / kPi; }

Transform IdentityTransform() {
  Transform out{};
  for (int i = 0; i < 4; ++i) {
    out.m[i][i] = 1.0;
  }
  return out;
}

Transform Multiply(Transform a, Transform b) {
  Transform out{};
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      for (int k = 0; k < 4; ++k) {
        out.m[r][c] += a.m[r][k] * b.m[k][c];
      }
    }
  }
  return out;
}

Transform PoseToTransform(arm_geometry::Pose3 pose) {
  // ZYX yaw-pitch-roll convention: R = Rz(yaw) * Ry(pitch) * Rx(roll).
  // Kris Hauser's Robotic Systems notes describe the same robotics convention:
  // https://motion.cs.illinois.edu/RoboticSystems/3DRotations.html
  const double cr = std::cos(pose.euler_rad.roll_rad);
  const double sr = std::sin(pose.euler_rad.roll_rad);
  const double cp = std::cos(pose.euler_rad.pitch_rad);
  const double sp = std::sin(pose.euler_rad.pitch_rad);
  const double cy = std::cos(pose.euler_rad.yaw_rad);
  const double sy = std::sin(pose.euler_rad.yaw_rad);

  Transform out = IdentityTransform();
  out.m[0][0] = cy * cp;
  out.m[0][1] = cy * sp * sr - sy * cr;
  out.m[0][2] = cy * sp * cr + sy * sr;
  out.m[1][0] = sy * cp;
  out.m[1][1] = sy * sp * sr + cy * cr;
  out.m[1][2] = sy * sp * cr - cy * sr;
  out.m[2][0] = -sp;
  out.m[2][1] = cp * sr;
  out.m[2][2] = cp * cr;
  out.m[0][3] = pose.position_m.x;
  out.m[1][3] = pose.position_m.y;
  out.m[2][3] = pose.position_m.z;
  return out;
}

Transform InverseRigidTransform(Transform transform) {
  Transform out = IdentityTransform();
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      out.m[r][c] = transform.m[c][r];
    }
  }

  const arm_geometry::Vec3 t{
      transform.m[0][3],
      transform.m[1][3],
      transform.m[2][3],
  };
  const arm_geometry::Vec3 inv_t = TransformPoint(out, {-t.x, -t.y, -t.z});
  out.m[0][3] = inv_t.x;
  out.m[1][3] = inv_t.y;
  out.m[2][3] = inv_t.z;
  return out;
}

arm_geometry::Pose3 TransformToPose(Transform transform) {
  arm_geometry::Pose3 pose;
  pose.position_m = {transform.m[0][3], transform.m[1][3],
                     transform.m[2][3]};

  const double pitch =
      std::atan2(-transform.m[2][0],
                 std::sqrt(transform.m[2][1] * transform.m[2][1] +
                           transform.m[2][2] * transform.m[2][2]));
  pose.euler_rad.roll_rad = std::atan2(transform.m[2][1], transform.m[2][2]);
  pose.euler_rad.pitch_rad = pitch;
  pose.euler_rad.yaw_rad = std::atan2(transform.m[1][0], transform.m[0][0]);
  return pose;
}

GeneratedRobotState GenerateRobotState(
    const arm_geometry::ArmGeometry& geometry,
    const std::vector<double>& joint_angles_deg) {
  GeneratedRobotState state;
  state.joint_angles_deg =
      arm_geometry::ClampJointAngles(geometry, joint_angles_deg);

  const std::vector<Transform> transforms =
      JointTransforms(geometry, state.joint_angles_deg);
  state.joint_poses.reserve(transforms.size());
  for (Transform transform : transforms) {
    state.joint_poses.push_back(TransformToPose(transform));
  }
  state.end_effector_pose = state.joint_poses.back();

  state.april_tag_poses.reserve(geometry.april_tags.size());
  for (const arm_geometry::AprilTagGeometry& tag : geometry.april_tags) {
    const int transform_index = tag.attached_after_joint + 1;
    if (transform_index < 0 ||
        transform_index >= static_cast<int>(transforms.size())) {
      throw std::runtime_error("AprilTag is attached to an invalid joint");
    }
    const Transform tag_transform =
        Multiply(transforms[transform_index], PoseToTransform(tag.local_pose));
    state.april_tag_poses.push_back(TransformToPose(tag_transform));
  }
  return state;
}

bool SolveIkPositionOnly(const arm_geometry::ArmGeometry& geometry,
                         arm_geometry::Vec3 target_xyz_m,
                         const std::vector<double>& initial_angles_deg,
                         int max_iters, double damping,
                         double tolerance_m,
                         std::vector<double>* solved_angles_deg,
                         double* final_error_m) {
  return SolveIkPositionOnlyInternal(
      geometry, target_xyz_m, initial_angles_deg, {}, max_iters, damping,
      tolerance_m, solved_angles_deg, final_error_m);
}

bool SolveIkPositionOnlyWithFixedJoints(
    const arm_geometry::ArmGeometry& geometry,
    arm_geometry::Vec3 target_xyz_m,
    const std::vector<double>& initial_angles_deg,
    const std::vector<bool>& fixed_joints,
    int max_iters, double damping,
    double tolerance_m,
    std::vector<double>* solved_angles_deg,
    double* final_error_m) {
  return SolveIkPositionOnlyInternal(
      geometry, target_xyz_m, initial_angles_deg, fixed_joints, max_iters,
      damping, tolerance_m, solved_angles_deg, final_error_m);
}

double Distance(arm_geometry::Vec3 a, arm_geometry::Vec3 b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  const double dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace arm_fk

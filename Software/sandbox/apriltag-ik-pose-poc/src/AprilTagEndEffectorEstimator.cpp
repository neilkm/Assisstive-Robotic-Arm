#include "AprilTagEndEffectorEstimator.hpp"

#include <cmath>
#include <map>
#include <stdexcept>

namespace {

double EulerDistance(arm_geometry::EulerAngles a, arm_geometry::EulerAngles b) {
  const double dr = a.roll_rad - b.roll_rad;
  const double dp = a.pitch_rad - b.pitch_rad;
  const double dy = a.yaw_rad - b.yaw_rad;
  return std::sqrt(dr * dr + dp * dp + dy * dy);
}

arm_geometry::Pose3 AveragePose(const std::vector<arm_geometry::Pose3>& poses) {
  arm_geometry::Pose3 out;
  for (const arm_geometry::Pose3& pose : poses) {
    out.position_m.x += pose.position_m.x;
    out.position_m.y += pose.position_m.y;
    out.position_m.z += pose.position_m.z;
    out.euler_rad.roll_rad += pose.euler_rad.roll_rad;
    out.euler_rad.pitch_rad += pose.euler_rad.pitch_rad;
    out.euler_rad.yaw_rad += pose.euler_rad.yaw_rad;
  }
  const double count = static_cast<double>(poses.size());
  out.position_m.x /= count;
  out.position_m.y /= count;
  out.position_m.z /= count;
  out.euler_rad.roll_rad /= count;
  out.euler_rad.pitch_rad /= count;
  out.euler_rad.yaw_rad /= count;
  return out;
}

}  // namespace

namespace arm_pose_estimator {

EstimateResult EstimateEndEffectorPose(
    const arm_geometry::ArmGeometry& geometry,
    const std::vector<AprilTagStatus>& tag_statuses) {
  EstimateResult result;
  std::map<std::string, AprilTagStatus> status_by_id;
  for (const AprilTagStatus& status : tag_statuses) {
    status_by_id[status.id] = status;
    if (status.is_visible) {
      ++result.visible_tag_count;
    }
  }

  std::vector<arm_geometry::Pose3> direct_end_effector_estimates;
  for (const arm_geometry::AprilTagGeometry& tag : geometry.april_tags) {
    const auto status_it = status_by_id.find(tag.id);
    if (status_it == status_by_id.end() || !status_it->second.is_visible) {
      continue;
    }

    if (tag.attached_after_joint !=
        static_cast<int>(geometry.joints.size()) - 1) {
      continue;
    }

    // A tag attached after the last joint gives a direct rigid transform:
    // T_base_ee = T_base_tag * inverse(T_ee_tag).
    const arm_fk::Transform base_to_tag =
        arm_fk::PoseToTransform(status_it->second.last_pose);
    const arm_fk::Transform ee_to_tag = arm_fk::PoseToTransform(tag.local_pose);
    direct_end_effector_estimates.push_back(arm_fk::TransformToPose(
        arm_fk::Multiply(base_to_tag, arm_fk::InverseRigidTransform(ee_to_tag))));
  }

  if (direct_end_effector_estimates.empty()) {
    result.warning =
        "No visible AprilTag is attached to the end effector; pose estimate is "
        "not observable in this POC without running nonlinear joint recovery.";
    result.converged = false;
    return result;
  }

  result.end_effector_pose = AveragePose(direct_end_effector_estimates);
  double squared_error = 0.0;
  for (const arm_geometry::Pose3& pose : direct_end_effector_estimates) {
    const double position_error =
        arm_fk::Distance(pose.position_m, result.end_effector_pose.position_m);
    const double rotation_error =
        EulerDistance(pose.euler_rad, result.end_effector_pose.euler_rad);
    squared_error += position_error * position_error +
                     rotation_error * rotation_error;
  }
  result.rms_error =
      std::sqrt(squared_error /
                static_cast<double>(direct_end_effector_estimates.size()));
  result.converged = true;
  if (direct_end_effector_estimates.size() == 1) {
    result.warning =
        "Pose estimated from one visible end-effector AprilTag; no redundancy "
        "check is available.";
  }
  return result;
}

}  // namespace arm_pose_estimator

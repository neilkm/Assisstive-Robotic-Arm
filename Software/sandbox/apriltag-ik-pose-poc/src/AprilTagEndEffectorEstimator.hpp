#pragma once

#include "ArmForwardKinematics.hpp"
#include "ArmGeometry.hpp"

#include <string>
#include <vector>

namespace arm_pose_estimator {

/** Last recorded AprilTag measurement supplied by an external detector. */
struct AprilTagStatus {
  std::string id;
  arm_geometry::Pose3 last_pose;
  bool is_visible = false;
};

/** End-effector pose estimate and diagnostic values. */
struct EstimateResult {
  arm_geometry::Pose3 end_effector_pose;
  int visible_tag_count = 0;
  bool converged = false;
  double rms_error = 0.0;
  std::string warning;
};

/** Estimate end-effector pose from visible AprilTag statuses and geometry. */
EstimateResult EstimateEndEffectorPose(
    const arm_geometry::ArmGeometry& geometry,
    const std::vector<AprilTagStatus>& tag_statuses);

}  // namespace arm_pose_estimator

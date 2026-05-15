#include "AprilTagEndEffectorEstimator.hpp"
#include "ArmForwardKinematics.hpp"
#include "ArmGeometry.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string ProjectPath(const std::string& relative) {
  const char* root = std::getenv("APRILTAG_IK_POSE_POC_ROOT");
  if (root != nullptr) {
    return std::string(root) + "/" + relative;
  }
  return relative;
}

void Require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void RequireNear(double actual, double expected, double tolerance,
                 const std::string& message) {
  if (std::fabs(actual - expected) > tolerance) {
    throw std::runtime_error(message + " actual=" + std::to_string(actual) +
                             " expected=" + std::to_string(expected));
  }
}

std::vector<arm_pose_estimator::AprilTagStatus> GenerateStatuses(
    const arm_geometry::ArmGeometry& geometry,
    const arm_fk::GeneratedRobotState& state) {
  std::vector<arm_pose_estimator::AprilTagStatus> statuses;
  for (size_t i = 0; i < geometry.april_tags.size(); ++i) {
    statuses.push_back(
        {geometry.april_tags[i].id, state.april_tag_poses[i], true});
  }
  return statuses;
}

void TestEndEffectorPoseCanBeCalculatedFromAprilTags() {
  const arm_geometry::ArmGeometry geometry =
      arm_geometry::LoadArmGeometry(ProjectPath("configs/arm_geometry.csv"));
  std::mt19937 rng(421);

  for (int sample = 0; sample < 20; ++sample) {
    std::vector<double> q;
    for (const arm_geometry::JointGeometry& joint : geometry.joints) {
      std::uniform_real_distribution<double> dist(joint.min_deg,
                                                  joint.max_deg);
      q.push_back(dist(rng));
    }
    const arm_fk::GeneratedRobotState state =
        arm_fk::GenerateRobotState(geometry, q);
    const std::vector<arm_pose_estimator::AprilTagStatus> statuses =
        GenerateStatuses(geometry, state);

    const arm_pose_estimator::EstimateResult estimate =
        arm_pose_estimator::EstimateEndEffectorPose(geometry, statuses);
    Require(estimate.converged, "estimator should converge with all tags");
    RequireNear(arm_fk::Distance(estimate.end_effector_pose.position_m,
                                 state.end_effector_pose.position_m),
                0.0, 1e-9, "end effector position from all tags");
  }
}

void TestOneVisibleEndEffectorTagIsEnoughForPose() {
  const arm_geometry::ArmGeometry geometry =
      arm_geometry::LoadArmGeometry(ProjectPath("configs/arm_geometry.csv"));
  const std::vector<double> q = {22.0, -18.0, 31.0, -44.0, 16.0, 83.0};
  const arm_fk::GeneratedRobotState state =
      arm_fk::GenerateRobotState(geometry, q);
  std::vector<arm_pose_estimator::AprilTagStatus> statuses =
      GenerateStatuses(geometry, state);

  for (arm_pose_estimator::AprilTagStatus& status : statuses) {
    status.is_visible = status.id == "apriltag6";
  }

  const arm_pose_estimator::EstimateResult estimate =
      arm_pose_estimator::EstimateEndEffectorPose(geometry, statuses);
  Require(estimate.converged,
          "one visible AprilTag attached to the end effector should solve pose");
  RequireNear(arm_fk::Distance(estimate.end_effector_pose.position_m,
                               state.end_effector_pose.position_m),
              0.0, 1e-9, "end effector position from one visible EE tag");
}

void TestEstimatorWarnsWhenNoEndEffectorTagIsVisible() {
  const arm_geometry::ArmGeometry geometry =
      arm_geometry::LoadArmGeometry(ProjectPath("configs/arm_geometry.csv"));
  const arm_fk::GeneratedRobotState state =
      arm_fk::GenerateRobotState(geometry, std::vector<double>(6, 0.0));
  std::vector<arm_pose_estimator::AprilTagStatus> statuses =
      GenerateStatuses(geometry, state);

  for (arm_pose_estimator::AprilTagStatus& status : statuses) {
    status.is_visible = status.id == "apriltag0";
  }

  const arm_pose_estimator::EstimateResult estimate =
      arm_pose_estimator::EstimateEndEffectorPose(geometry, statuses);
  Require(!estimate.converged,
          "estimator should warn when visible tags cannot observe EE pose");
  Require(!estimate.warning.empty(), "estimator warning should be populated");
}

}  // namespace

int main() {
  try {
    TestEndEffectorPoseCanBeCalculatedFromAprilTags();
    TestOneVisibleEndEffectorTagIsEnoughForPose();
    TestEstimatorWarnsWhenNoEndEffectorTagIsVisible();
  } catch (const std::exception& e) {
    std::cerr << "FAILED: " << e.what() << "\n";
    return 1;
  }
  std::cout << "All AprilTag end-effector estimator tests passed.\n";
  return 0;
}

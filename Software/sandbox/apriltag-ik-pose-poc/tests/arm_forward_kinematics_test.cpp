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

void PrintPose(const std::string& label, arm_geometry::Pose3 pose) {
  std::cout << label << " xyz=[" << pose.position_m.x << ", "
            << pose.position_m.y << ", " << pose.position_m.z << "] rpy=["
            << arm_fk::RadToDeg(pose.euler_rad.roll_rad) << ", "
            << arm_fk::RadToDeg(pose.euler_rad.pitch_rad) << ", "
            << arm_fk::RadToDeg(pose.euler_rad.yaw_rad) << "] deg\n";
}

arm_geometry::ArmGeometry LoadGeometry() {
  return arm_geometry::LoadArmGeometry(ProjectPath("configs/arm_geometry.csv"));
}

void TestGeometryFileLoadsEverything() {
  const arm_geometry::ArmGeometry geometry = LoadGeometry();
  Require(geometry.dimensions.size() == 10, "geometry dimension count");
  Require(geometry.joints.size() == 6, "geometry joint count");
  Require(geometry.april_tags.size() == 8, "geometry AprilTag count");
  RequireNear(geometry.dimensions.at("L0"), 1.0, 1e-9, "L0 default");
  RequireNear(geometry.dimensions.at("W2"), 1.0, 1e-9, "W2 default");
}

void TestForwardKinematicsGetsEndEffectorToSolvedTarget() {
  const arm_geometry::ArmGeometry geometry = LoadGeometry();
  const std::vector<double> zero(6, 0.0);
  const arm_geometry::Vec3 zero_target =
      arm_fk::GenerateRobotState(geometry, zero).end_effector_pose.position_m;

  std::vector<double> solved;
  double error_m = 0.0;
  const bool converged =
      arm_fk::SolveIkPositionOnly(geometry, zero_target,
                                  arm_geometry::InitialJointAngles(geometry),
                                  60, 0.035, 0.001, &solved, &error_m);
  const arm_geometry::Pose3 reached =
      arm_fk::GenerateRobotState(geometry, solved).end_effector_pose;

  Require(converged, "IK should converge to a known zero-state target");
  RequireNear(arm_fk::Distance(zero_target, reached.position_m), 0.0, 1e-9,
              "FK reached target after IK");
}

void TestForwardKinematicsReachesTargetWithFixedJoint5Approach() {
  const arm_geometry::ArmGeometry geometry = LoadGeometry();
  const std::vector<double> target_angles = {12.0, -9.0, 8.0,
                                             -6.0, 4.0, 35.0};
  const arm_geometry::Vec3 target =
      arm_fk::GenerateRobotState(geometry, target_angles)
          .end_effector_pose.position_m;

  std::vector<double> initial = arm_geometry::InitialJointAngles(geometry);
  initial[5] = target_angles[5];
  std::vector<bool> fixed_joints(initial.size(), false);
  fixed_joints[5] = true;

  std::vector<double> solved;
  double error_m = 0.0;
  const bool converged = arm_fk::SolveIkPositionOnlyWithFixedJoints(
      geometry, target, initial, fixed_joints, 180, 0.035, 0.001, &solved,
      &error_m);
  const arm_geometry::Pose3 reached =
      arm_fk::GenerateRobotState(geometry, solved).end_effector_pose;

  Require(converged, "IK should converge with joint5 fixed");
  RequireNear(solved[5], target_angles[5], 1e-9, "fixed joint5 angle");
  RequireNear(arm_fk::Distance(target, reached.position_m), 0.0, 0.001,
              "FK reached target while preserving approach angle");
}

void TestRandomJointAnglesGenerateTagAndEndEffectorPoses() {
  const arm_geometry::ArmGeometry geometry = LoadGeometry();
  std::mt19937 rng(129);
  for (int sample = 0; sample < 12; ++sample) {
    std::vector<double> q;
    for (const arm_geometry::JointGeometry& joint : geometry.joints) {
      std::uniform_real_distribution<double> dist(joint.min_deg,
                                                  joint.max_deg);
      q.push_back(dist(rng));
    }

    const arm_fk::GeneratedRobotState state =
        arm_fk::GenerateRobotState(geometry, q);
    Require(state.april_tag_poses.size() == geometry.april_tags.size(),
            "random FK tag count");

    if (sample < 3) {
      PrintPose("sample" + std::to_string(sample) + " end_effector",
                state.end_effector_pose);
      PrintPose("sample" + std::to_string(sample) + " apriltag0",
                state.april_tag_poses.front());
    }
  }
}

}  // namespace

int main() {
  try {
    TestGeometryFileLoadsEverything();
    TestForwardKinematicsGetsEndEffectorToSolvedTarget();
    TestForwardKinematicsReachesTargetWithFixedJoint5Approach();
    TestRandomJointAnglesGenerateTagAndEndEffectorPoses();
  } catch (const std::exception& e) {
    std::cerr << "FAILED: " << e.what() << "\n";
    return 1;
  }
  std::cout << "All arm forward kinematics tests passed.\n";
  return 0;
}

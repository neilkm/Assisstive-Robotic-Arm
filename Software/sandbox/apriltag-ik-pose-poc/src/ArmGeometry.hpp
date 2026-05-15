#pragma once

#include <map>
#include <string>
#include <vector>

namespace arm_geometry {

/** Cartesian vector in meters. */
struct Vec3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

/** ZYX yaw-pitch-roll Euler angles in radians. */
struct EulerAngles {
  double roll_rad = 0.0;
  double pitch_rad = 0.0;
  double yaw_rad = 0.0;
};

/** Rigid pose represented by position plus ZYX Euler angles. */
struct Pose3 {
  Vec3 position_m;
  EulerAngles euler_rad;
};

/** One standard Denavit-Hartenberg revolute joint row. */
struct JointGeometry {
  std::string name;
  double a_m = 0.0;
  double alpha_rad = 0.0;
  double d_m = 0.0;
  double theta_offset_rad = 0.0;
  double initial_deg = 0.0;
  double min_deg = 0.0;
  double max_deg = 0.0;
};

/** AprilTag local pose relative to the transform after a joint. */
struct AprilTagGeometry {
  std::string id;
  int attached_after_joint = 0;
  Pose3 local_pose;
};

/** Complete robot geometry loaded from the single arm_geometry.csv file. */
struct ArmGeometry {
  std::map<std::string, double> dimensions;
  std::vector<JointGeometry> joints;
  std::vector<AprilTagGeometry> april_tags;
};

/** Load dimensions, DH rows, and AprilTag local poses from one CSV file. */
ArmGeometry LoadArmGeometry(const std::string& path);

/** Utility used by tests and controllers to clamp joint angles to CSV limits. */
std::vector<double> ClampJointAngles(const ArmGeometry& geometry,
                                     const std::vector<double>& joint_angles_deg);

/** Return initial joint angles from the geometry file. */
std::vector<double> InitialJointAngles(const ArmGeometry& geometry);

}  // namespace arm_geometry

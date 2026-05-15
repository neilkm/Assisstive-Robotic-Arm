#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

namespace arm {

struct Vec3 {
    double x;
    double y;
    double z;
};

struct Mat4 {
    double m[4][4];
};

struct JointSpec {
    std::string name;
    double a_m;
    double alpha_rad;
    double d_m;
    double theta_offset_rad;
    double initial_deg;
    double min_deg;
    double max_deg;
};

struct TagSpec {
    std::string id;
    int attached_after_joint;
    Vec3 local_xyz_m;
};

struct IkResult {
    std::vector<double> q_deg;
    bool converged;
    double error_m;
};

struct TagPoseResult {
    std::vector<double> q_deg;
    Vec3 end_effector_xyz_m;
    bool converged;
    double rms_error_m;
};

std::map<std::string, double> load_dimension_table(const std::string& path);
std::vector<JointSpec> load_dh_table(const std::string& path, const std::map<std::string, double>& dimensions);
std::vector<TagSpec> load_tag_table(const std::string& path, const std::map<std::string, double>& dimensions);

std::vector<double> initial_joint_angles_deg(const std::vector<JointSpec>& joints);
std::vector<double> clamp_to_limits(const std::vector<JointSpec>& joints, const std::vector<double>& q_deg);
std::vector<Mat4> joint_transforms(const std::vector<JointSpec>& joints, const std::vector<double>& q_deg);
std::vector<Vec3> chain_points(const std::vector<JointSpec>& joints, const std::vector<double>& q_deg);
Vec3 end_effector_position(const std::vector<JointSpec>& joints, const std::vector<double>& q_deg);
IkResult solve_ik_position_only(
    const std::vector<JointSpec>& joints,
    Vec3 target_xyz_m,
    const std::vector<double>& q_init_deg,
    int max_iters,
    double damping,
    double tolerance_m);

std::vector<Vec3> tag_centers_from_joint_angles(
    const std::vector<JointSpec>& joints,
    const std::vector<TagSpec>& tags,
    const std::vector<double>& q_deg);
TagPoseResult estimate_pose_from_tags(
    const std::vector<JointSpec>& joints,
    const std::vector<TagSpec>& tags,
    const std::vector<Vec3>& observed_centers,
    const std::vector<double>& q_init_deg,
    int max_iters,
    double damping,
    double tolerance_m);

double distance(Vec3 a, Vec3 b);

}  // namespace arm

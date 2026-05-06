#include "kinematics.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

constexpr int kIkMaxIters = 180;
constexpr double kIkDamping = 0.035;
constexpr double kIkToleranceM = 0.001;
constexpr int kTagMaxIters = 240;
constexpr double kTagDamping = 0.025;
constexpr double kTagToleranceM = 0.001;
constexpr double kMotionSpeedDegPerS = 5.0;

std::string project_path(const std::string& relative) {
    const char* root = std::getenv("APRILTAG_IK_POSE_POC_ROOT");
    if (root != nullptr) {
        return std::string(root) + "/" + relative;
    }
    return relative;
}

void print_vec3(std::ostream& os, arm::Vec3 v) {
    os << "[" << v.x << "," << v.y << "," << v.z << "]";
}

void print_vec3_array(std::ostream& os, const std::vector<arm::Vec3>& values) {
    os << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << ",";
        }
        print_vec3(os, values[i]);
    }
    os << "]";
}

void print_number_array(std::ostream& os, const std::vector<double>& values) {
    os << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << ",";
        }
        os << values[i];
    }
    os << "]";
}

void print_string_array(std::ostream& os, const std::vector<std::string>& values) {
    os << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << ",";
        }
        os << "\"" << values[i] << "\"";
    }
    os << "]";
}

void print_frame_json(
    std::ostream& os,
    const std::vector<arm::JointSpec>& joints,
    const std::vector<arm::TagSpec>& tags,
    const std::vector<double>& q_deg) {
    const std::vector<double> q_clamped = arm::clamp_to_limits(joints, q_deg);
    const std::vector<arm::Vec3> tag_centers = arm::tag_centers_from_joint_angles(joints, tags, q_clamped);
    const arm::Vec3 ee = arm::end_effector_position(joints, q_clamped);

    os << "{";
    os << "\"joint_angles_deg\":";
    print_number_array(os, q_clamped);
    os << ",\"chain_points_m\":";
    print_vec3_array(os, arm::chain_points(joints, q_clamped));
    os << ",\"tag_centers_m\":";
    print_vec3_array(os, tag_centers);
    os << ",\"actual_ee_xyz_m\":";
    print_vec3(os, ee);
    os << ",\"tag_ee_xyz_m\":";
    print_vec3(os, ee);
    os << "}";
}

void print_pose_json(
    const std::vector<arm::JointSpec>& joints,
    const std::vector<arm::TagSpec>& tags,
    const std::vector<double>& q_deg,
    arm::Vec3 target,
    bool ik_converged,
    double ik_error_m,
    const arm::TagPoseResult& tag_pose,
    const std::vector<arm::Vec3>& observed_tags) {
    const arm::Vec3 actual_ee = arm::end_effector_position(joints, q_deg);
    const double ee_compare_error = arm::distance(actual_ee, tag_pose.end_effector_xyz_m);
    std::vector<std::string> joint_names;
    for (const arm::JointSpec& joint : joints) {
        joint_names.push_back(joint.name);
    }
    std::vector<std::string> tag_ids;
    for (const arm::TagSpec& tag : tags) {
        tag_ids.push_back(tag.id);
    }

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "{";
    std::cout << "\"ik_converged\":" << (ik_converged ? "true" : "false") << ",";
    std::cout << "\"ik_error_m\":" << ik_error_m << ",";
    std::cout << "\"tag_converged\":" << (tag_pose.converged ? "true" : "false") << ",";
    std::cout << "\"tag_rms_error_m\":" << tag_pose.rms_error_m << ",";
    std::cout << "\"ee_compare_error_m\":" << ee_compare_error << ",";
    std::cout << "\"motion_speed_deg_per_s\":" << kMotionSpeedDegPerS << ",";
    std::cout << "\"joint_names\":";
    print_string_array(std::cout, joint_names);
    std::cout << ",\"joint_angles_deg\":";
    print_number_array(std::cout, q_deg);
    std::cout << ",\"tag_joint_angles_deg\":";
    print_number_array(std::cout, tag_pose.q_deg);
    std::cout << ",\"chain_points_m\":";
    print_vec3_array(std::cout, arm::chain_points(joints, q_deg));
    std::cout << ",\"tag_ids\":";
    print_string_array(std::cout, tag_ids);
    std::cout << ",\"tag_centers_m\":";
    print_vec3_array(std::cout, observed_tags);
    std::cout << ",\"target_xyz_m\":";
    print_vec3(std::cout, target);
    std::cout << ",\"actual_ee_xyz_m\":";
    print_vec3(std::cout, actual_ee);
    std::cout << ",\"tag_ee_xyz_m\":";
    print_vec3(std::cout, tag_pose.end_effector_xyz_m);
    std::cout << "}\n";
}

int run_case(arm::Vec3 target, bool smoke) {
    const std::map<std::string, double> dimensions =
        arm::load_dimension_table(project_path("configs/robot_dimensions.csv"));
    const std::vector<arm::JointSpec> joints =
        arm::load_dh_table(project_path("configs/dh_table.csv"), dimensions);
    const std::vector<arm::TagSpec> tags =
        arm::load_tag_table(project_path("configs/apriltags.csv"), dimensions);
    const std::vector<double> q0 = arm::initial_joint_angles_deg(joints);

    const arm::IkResult ik = arm::solve_ik_position_only(
        joints, target, q0, kIkMaxIters, kIkDamping, kIkToleranceM);
    const std::vector<arm::Vec3> observed_tags = arm::tag_centers_from_joint_angles(joints, tags, ik.q_deg);
    const arm::TagPoseResult tag_pose = arm::estimate_pose_from_tags(
        joints, tags, observed_tags, q0, kTagMaxIters, kTagDamping, kTagToleranceM);
    const arm::Vec3 actual_ee = arm::end_effector_position(joints, ik.q_deg);
    const double ee_compare_error = arm::distance(actual_ee, tag_pose.end_effector_xyz_m);

    if (smoke) {
        std::cout << "IK converged: " << (ik.converged ? "true" : "false") << "\n";
        std::cout << "IK target error m: " << ik.error_m << "\n";
        std::cout << "Tag pose converged: " << (tag_pose.converged ? "true" : "false") << "\n";
        std::cout << "Tag RMS error m: " << tag_pose.rms_error_m << "\n";
        std::cout << "EE comparison error m: " << ee_compare_error << "\n";
        if (!ik.converged || !tag_pose.converged || ee_compare_error > 0.003) {
            return 1;
        }
        return 0;
    }

    print_pose_json(joints, tags, ik.q_deg, target, ik.converged, ik.error_m, tag_pose, observed_tags);
    return 0;
}

int run_zero_state(bool smoke) {
    const std::map<std::string, double> dimensions =
        arm::load_dimension_table(project_path("configs/robot_dimensions.csv"));
    const std::vector<arm::JointSpec> joints =
        arm::load_dh_table(project_path("configs/dh_table.csv"), dimensions);
    const std::vector<arm::TagSpec> tags =
        arm::load_tag_table(project_path("configs/apriltags.csv"), dimensions);
    const std::vector<double> q_zero(joints.size(), 0.0);
    const std::vector<arm::Vec3> observed_tags = arm::tag_centers_from_joint_angles(joints, tags, q_zero);
    const arm::TagPoseResult tag_pose = arm::estimate_pose_from_tags(
        joints, tags, observed_tags, q_zero, kTagMaxIters, kTagDamping, kTagToleranceM);
    const arm::Vec3 ee = arm::end_effector_position(joints, q_zero);
    const double ee_compare_error = arm::distance(ee, tag_pose.end_effector_xyz_m);

    if (smoke) {
        std::cout << "Zero-state tag pose converged: " << (tag_pose.converged ? "true" : "false") << "\n";
        std::cout << "Zero-state tag RMS error m: " << tag_pose.rms_error_m << "\n";
        std::cout << "Zero-state EE comparison error m: " << ee_compare_error << "\n";
        if (!tag_pose.converged || ee_compare_error > 0.001) {
            return 1;
        }
        return 0;
    }

    print_pose_json(joints, tags, q_zero, ee, true, 0.0, tag_pose, observed_tags);
    return 0;
}

int run_angles(const std::vector<double>& q_deg) {
    const std::map<std::string, double> dimensions =
        arm::load_dimension_table(project_path("configs/robot_dimensions.csv"));
    const std::vector<arm::JointSpec> joints =
        arm::load_dh_table(project_path("configs/dh_table.csv"), dimensions);
    const std::vector<arm::TagSpec> tags =
        arm::load_tag_table(project_path("configs/apriltags.csv"), dimensions);
    const std::vector<double> q_clamped = arm::clamp_to_limits(joints, q_deg);
    const std::vector<arm::Vec3> observed_tags = arm::tag_centers_from_joint_angles(joints, tags, q_clamped);
    const arm::Vec3 ee = arm::end_effector_position(joints, q_clamped);
    const arm::TagPoseResult tag_pose{q_clamped, ee, true, 0.0};
    print_pose_json(joints, tags, q_clamped, ee, true, 0.0, tag_pose, observed_tags);
    return 0;
}

int run_trajectory(
    const std::vector<double>& q_start_deg,
    const std::vector<double>& q_goal_deg,
    double speed_deg_per_s,
    double fps) {
    const std::map<std::string, double> dimensions =
        arm::load_dimension_table(project_path("configs/robot_dimensions.csv"));
    const std::vector<arm::JointSpec> joints =
        arm::load_dh_table(project_path("configs/dh_table.csv"), dimensions);
    const std::vector<arm::TagSpec> tags =
        arm::load_tag_table(project_path("configs/apriltags.csv"), dimensions);
    if (q_start_deg.size() != joints.size() || q_goal_deg.size() != joints.size()) {
        throw std::runtime_error("Trajectory angle count must match the 6-joint DH table");
    }

    speed_deg_per_s = std::max(0.1, speed_deg_per_s);
    fps = std::max(1.0, std::min(fps, 30.0));

    double max_delta_deg = 0.0;
    for (size_t i = 0; i < q_start_deg.size(); ++i) {
        max_delta_deg = std::max(max_delta_deg, std::fabs(q_goal_deg[i] - q_start_deg[i]));
    }
    const double duration_s = max_delta_deg / speed_deg_per_s;
    const int frames = std::max(2, static_cast<int>(std::ceil(duration_s * fps)));

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "{\"duration_s\":" << duration_s << ",\"fps\":" << fps << ",\"frames\":[";
    for (int frame = 1; frame <= frames; ++frame) {
        if (frame != 1) {
            std::cout << ",";
        }
        const double t = static_cast<double>(frame) / static_cast<double>(frames);
        std::vector<double> q_step(q_start_deg.size(), 0.0);
        for (size_t i = 0; i < q_step.size(); ++i) {
            q_step[i] = q_start_deg[i] + t * (q_goal_deg[i] - q_start_deg[i]);
        }
        print_frame_json(std::cout, joints, tags, q_step);
    }
    std::cout << "]}\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--smoke") {
            return run_zero_state(true);
        }
        if (argc == 2 && std::string(argv[1]) == "--zero") {
            return run_zero_state(false);
        }
        if (argc == 5 && std::string(argv[1]) == "--target") {
            return run_case({std::stod(argv[2]), std::stod(argv[3]), std::stod(argv[4])}, false);
        }
        if (argc == 8 && std::string(argv[1]) == "--angles") {
            return run_angles({
                std::stod(argv[2]),
                std::stod(argv[3]),
                std::stod(argv[4]),
                std::stod(argv[5]),
                std::stod(argv[6]),
                std::stod(argv[7]),
            });
        }
        if (argc == 16 && std::string(argv[1]) == "--trajectory") {
            return run_trajectory(
                {
                    std::stod(argv[2]),
                    std::stod(argv[3]),
                    std::stod(argv[4]),
                    std::stod(argv[5]),
                    std::stod(argv[6]),
                    std::stod(argv[7]),
                },
                {
                    std::stod(argv[8]),
                    std::stod(argv[9]),
                    std::stod(argv[10]),
                    std::stod(argv[11]),
                    std::stod(argv[12]),
                    std::stod(argv[13]),
                },
                std::stod(argv[14]),
                std::stod(argv[15]));
        }

        std::cerr << "Usage:\n";
        std::cerr << "  apriltag_ik_pose_backend --target X Y Z\n";
        std::cerr << "  apriltag_ik_pose_backend --angles J0 J1 J2 J3 J4 J5\n";
        std::cerr << "  apriltag_ik_pose_backend --trajectory START6 GOAL6 SPEED_DEG_PER_S FPS\n";
        std::cerr << "  apriltag_ik_pose_backend --smoke\n";
        std::cerr << "  apriltag_ik_pose_backend --zero\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

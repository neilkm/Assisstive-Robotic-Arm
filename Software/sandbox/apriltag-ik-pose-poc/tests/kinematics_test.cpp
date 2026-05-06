#include "kinematics.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string project_path(const std::string& relative) {
    const char* root = std::getenv("APRILTAG_IK_POSE_POC_ROOT");
    if (root != nullptr) {
        return std::string(root) + "/" + relative;
    }
    return relative;
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_near(double actual, double expected, double tolerance, const std::string& message) {
    if (std::fabs(actual - expected) > tolerance) {
        throw std::runtime_error(message + " actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
    }
}

void require_vec_near(arm::Vec3 actual, arm::Vec3 expected, double tolerance, const std::string& message) {
    require_near(actual.x, expected.x, tolerance, message + ".x");
    require_near(actual.y, expected.y, tolerance, message + ".y");
    require_near(actual.z, expected.z, tolerance, message + ".z");
}

struct TestFixture {
    std::map<std::string, double> dimensions;
    std::vector<arm::JointSpec> joints;
    std::vector<arm::TagSpec> tags;

    TestFixture()
        : dimensions(arm::load_dimension_table(project_path("configs/robot_dimensions.csv"))),
          joints(arm::load_dh_table(project_path("configs/dh_table.csv"), dimensions)),
          tags(arm::load_tag_table(project_path("configs/apriltags.csv"), dimensions)) {}
};

void test_config_loads_six_joints_and_eight_tags() {
    TestFixture f;
    require(f.joints.size() == 6, "DH table should contain exactly 6 joints");
    require(f.tags.size() == 8, "AprilTag table should contain exactly 8 tags");
    const std::map<std::string, double> expected_defaults = {
        {"L0", 1.0},
        {"L1", 3.0},
        {"L2", 1.0},
        {"L3", 3.0},
        {"L4", 1.0},
        {"L5", 2.0},
        {"W0", 1.0},
        {"W1", 1.0},
        {"W2", 1.0},
        {"G0", 2.0},
    };
    for (const auto& [name, expected] : expected_defaults) {
        require(f.dimensions.count(name) == 1, "Dimension table should contain " + name);
        require_near(f.dimensions.at(name), expected, 1e-9, name + " default dimension");
    }
}

void test_zero_state_joint_centers_match_requested_geometry() {
    TestFixture f;
    const std::vector<double> q_zero(6, 0.0);
    const std::vector<arm::Vec3> points = arm::chain_points(f.joints, q_zero);
    const double L0 = f.dimensions.at("L0");
    const double L1 = f.dimensions.at("L1");
    const double L2 = f.dimensions.at("L2");
    const double L3 = f.dimensions.at("L3");
    const double L4 = f.dimensions.at("L4");
    const double L5 = f.dimensions.at("L5");

    require(points.size() == 7, "6 joints should produce 7 chain points including the base and end effector");
    require_vec_near(points[0], {0.0, 0.0, 0.0}, 1e-9, "joint0");
    require_vec_near(points[1], {L0, 0.0, 0.0}, 1e-9, "joint1");
    require_vec_near(points[2], {L0, 0.0, L1}, 1e-9, "joint2");
    require_vec_near(points[3], {L0, 0.0, L1 + L2}, 1e-9, "joint3");
    require_vec_near(points[4], {L0, 0.0, L1 + L2 + L3}, 1e-9, "joint4");
    require_vec_near(points[5], {L0, 0.0, L1 + L2 + L3 + L4}, 1e-9, "joint5");
    require_vec_near(points[6], {L0 + L5, 0.0, L1 + L2 + L3 + L4}, 1e-9, "end effector");
}

void test_zero_state_apriltag_centers_match_requested_geometry() {
    TestFixture f;
    const std::vector<double> q_zero(6, 0.0);
    const std::vector<arm::Vec3> tags = arm::tag_centers_from_joint_angles(f.joints, f.tags, q_zero);
    const double L0 = f.dimensions.at("L0");
    const double L1 = f.dimensions.at("L1");
    const double L2 = f.dimensions.at("L2");
    const double L3 = f.dimensions.at("L3");
    const double L4 = f.dimensions.at("L4");
    const double L5 = f.dimensions.at("L5");
    const double W0 = f.dimensions.at("W0");
    const double W1 = f.dimensions.at("W1");
    const double W2 = f.dimensions.at("W2");
    const double G0 = f.dimensions.at("G0");

    require_vec_near(tags[0], {L0 + W0, 0.0, 0.5 * L1}, 1e-9, "apriltag0");
    require_vec_near(tags[1], {L0 - W0, 0.0, 0.5 * L1}, 1e-9, "apriltag1");
    require_vec_near(tags[2], {L0, W1, L1 + L2 + 0.5 * L3}, 1e-9, "apriltag2");
    require_vec_near(tags[3], {L0, -W1, L1 + L2 + 0.5 * L3}, 1e-9, "apriltag3");
    require_vec_near(tags[4], {L0, W2, L1 + L2 + L3 + L4}, 1e-9, "apriltag4");
    require_vec_near(tags[5], {L0, -W2, L1 + L2 + L3 + L4}, 1e-9, "apriltag5");
    require_vec_near(tags[6], {L0 + L5, G0, L1 + L2 + L3 + L4}, 1e-9, "apriltag6");
    require_vec_near(tags[7], {L0 + L5, -G0, L1 + L2 + L3 + L4}, 1e-9, "apriltag7");
}

void test_clamping_and_zero_target_ik() {
    TestFixture f;
    const std::vector<double> unclamped = {999.0, -999.0, 0.0, 0.0, 0.0, 999.0};
    const std::vector<double> clamped = arm::clamp_to_limits(f.joints, unclamped);
    require_near(clamped[0], f.joints[0].max_deg, 1e-9, "joint0 clamp max");
    require_near(clamped[1], f.joints[1].min_deg, 1e-9, "joint1 clamp min");
    require_near(clamped[5], f.joints[5].max_deg, 1e-9, "joint5 clamp max");

    const std::vector<double> q_zero(6, 0.0);
    const arm::Vec3 target = arm::end_effector_position(f.joints, q_zero);
    const arm::IkResult ik = arm::solve_ik_position_only(f.joints, target, q_zero, 10, 0.035, 0.001);
    require(ik.converged, "IK should converge immediately for zero-state end-effector target");
    require_near(ik.error_m, 0.0, 1e-9, "IK zero-state error");
}

void test_zero_state_tag_pose_estimator() {
    TestFixture f;
    const std::vector<double> q_zero(6, 0.0);
    const std::vector<arm::Vec3> observed = arm::tag_centers_from_joint_angles(f.joints, f.tags, q_zero);
    const arm::TagPoseResult result = arm::estimate_pose_from_tags(f.joints, f.tags, observed, q_zero, 20, 0.025, 0.001);
    const arm::Vec3 expected_ee = arm::end_effector_position(f.joints, q_zero);
    require(result.converged, "Tag pose estimator should converge for zero-state simulated observations");
    require_vec_near(result.end_effector_xyz_m, expected_ee, 1e-9, "tag-derived zero-state end effector");
}

}  // namespace

int main() {
    try {
        test_config_loads_six_joints_and_eight_tags();
        test_zero_state_joint_centers_match_requested_geometry();
        test_zero_state_apriltag_centers_match_requested_geometry();
        test_clamping_and_zero_target_ik();
        test_zero_state_tag_pose_estimator();
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }

    std::cout << "All C++ kinematics unit tests passed.\n";
    return 0;
}

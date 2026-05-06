#include "kinematics.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace arm {
namespace {

constexpr double kPi = 3.14159265358979323846;

double deg_to_rad(double deg) {
    return deg * kPi / 180.0;
}

double rad_to_deg(double rad) {
    return rad * 180.0 / kPi;
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        out.push_back(item);
    }
    return out;
}

std::string trim(const std::string& in) {
    const size_t first = in.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const size_t last = in.find_last_not_of(" \t\r\n");
    return in.substr(first, last - first + 1);
}

double parse_numeric_or_symbol(std::string token, const std::map<std::string, double>& dimensions) {
    token = trim(token);
    if (token.empty()) {
        throw std::runtime_error("Empty numeric token in config");
    }

    size_t parsed_chars = 0;
    try {
        const double value = std::stod(token, &parsed_chars);
        if (parsed_chars == token.size()) {
            return value;
        }
    } catch (const std::exception&) {
    }

    const size_t multiply_pos = token.find('*');
    if (multiply_pos != std::string::npos) {
        const double lhs = parse_numeric_or_symbol(token.substr(0, multiply_pos), dimensions);
        const double rhs = parse_numeric_or_symbol(token.substr(multiply_pos + 1), dimensions);
        return lhs * rhs;
    }

    double sign = 1.0;
    if (token[0] == '-') {
        sign = -1.0;
        token = token.substr(1);
    } else if (token[0] == '+') {
        token = token.substr(1);
    }

    const auto it = dimensions.find(token);
    if (it == dimensions.end()) {
        throw std::runtime_error("Unknown dimension symbol: " + token);
    }
    return sign * it->second;
}

Mat4 identity4() {
    Mat4 out{};
    for (int i = 0; i < 4; ++i) {
        out.m[i][i] = 1.0;
    }
    return out;
}

Mat4 multiply(Mat4 a, Mat4 b) {
    Mat4 out{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            for (int k = 0; k < 4; ++k) {
                out.m[r][c] += a.m[r][k] * b.m[k][c];
            }
        }
    }
    return out;
}

Vec3 transform_point(Mat4 t, Vec3 p) {
    return {
        t.m[0][0] * p.x + t.m[0][1] * p.y + t.m[0][2] * p.z + t.m[0][3],
        t.m[1][0] * p.x + t.m[1][1] * p.y + t.m[1][2] * p.z + t.m[1][3],
        t.m[2][0] * p.x + t.m[2][1] * p.y + t.m[2][2] * p.z + t.m[2][3],
    };
}

Mat4 dh_transform(double a_m, double alpha_rad, double d_m, double theta_rad) {
    const double cth = std::cos(theta_rad);
    const double sth = std::sin(theta_rad);
    const double cal = std::cos(alpha_rad);
    const double sal = std::sin(alpha_rad);
    Mat4 out{};
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

std::vector<double> solve_linear_system(std::vector<std::vector<double>> a, std::vector<double> b) {
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

std::vector<std::vector<double>> position_jacobian(
    const std::vector<JointSpec>& joints,
    const std::vector<double>& q_deg,
    double eps_deg = 0.1) {
    const Vec3 p0 = end_effector_position(joints, q_deg);
    std::vector<std::vector<double>> jac(3, std::vector<double>(q_deg.size(), 0.0));
    for (size_t i = 0; i < q_deg.size(); ++i) {
        std::vector<double> q_step = q_deg;
        q_step[i] += eps_deg;
        const Vec3 p1 = end_effector_position(joints, q_step);
        const double eps_rad = deg_to_rad(eps_deg);
        jac[0][i] = (p1.x - p0.x) / eps_rad;
        jac[1][i] = (p1.y - p0.y) / eps_rad;
        jac[2][i] = (p1.z - p0.z) / eps_rad;
    }
    return jac;
}

std::vector<double> tag_error_vector(
    const std::vector<JointSpec>& joints,
    const std::vector<TagSpec>& tags,
    const std::vector<Vec3>& observed,
    const std::vector<double>& q_deg) {
    const std::vector<Vec3> predicted = tag_centers_from_joint_angles(joints, tags, q_deg);
    std::vector<double> error;
    error.reserve(predicted.size() * 3);
    for (size_t i = 0; i < predicted.size(); ++i) {
        error.push_back(observed[i].x - predicted[i].x);
        error.push_back(observed[i].y - predicted[i].y);
        error.push_back(observed[i].z - predicted[i].z);
    }
    return error;
}

std::vector<std::vector<double>> tag_jacobian(
    const std::vector<JointSpec>& joints,
    const std::vector<TagSpec>& tags,
    const std::vector<Vec3>& observed,
    const std::vector<double>& q_deg,
    double eps_deg = 0.1) {
    const std::vector<double> e0 = tag_error_vector(joints, tags, observed, q_deg);
    std::vector<std::vector<double>> jac(e0.size(), std::vector<double>(q_deg.size(), 0.0));
    for (size_t i = 0; i < q_deg.size(); ++i) {
        std::vector<double> q_step = q_deg;
        q_step[i] += eps_deg;
        const std::vector<double> e1 = tag_error_vector(joints, tags, observed, q_step);
        const double eps_rad = deg_to_rad(eps_deg);
        for (size_t row = 0; row < e0.size(); ++row) {
            jac[row][i] = (e1[row] - e0[row]) / eps_rad;
        }
    }
    return jac;
}

double norm3(Vec3 v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

double rms(const std::vector<double>& values) {
    double sum = 0.0;
    for (double v : values) {
        sum += v * v;
    }
    return std::sqrt(sum / static_cast<double>(values.size()));
}

}  // namespace

std::map<std::string, double> load_dimension_table(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("Could not open dimension table: " + path);
    }

    std::string line;
    std::getline(f, line);
    std::map<std::string, double> dimensions;
    while (std::getline(f, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> c = split_csv_line(line);
        if (c.size() < 2) {
            throw std::runtime_error("Invalid dimension row: " + line);
        }
        dimensions[trim(c[0])] = std::stod(trim(c[1]));
    }
    return dimensions;
}

std::vector<JointSpec> load_dh_table(const std::string& path, const std::map<std::string, double>& dimensions) {
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("Could not open DH table: " + path);
    }

    std::string line;
    std::getline(f, line);
    std::vector<JointSpec> joints;
    while (std::getline(f, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> c = split_csv_line(line);
        if (c.size() != 8) {
            throw std::runtime_error("Invalid DH row: " + line);
        }
        joints.push_back({
            c[0],
            parse_numeric_or_symbol(c[1], dimensions),
            parse_numeric_or_symbol(c[2], dimensions),
            parse_numeric_or_symbol(c[3], dimensions),
            parse_numeric_or_symbol(c[4], dimensions),
            parse_numeric_or_symbol(c[5], dimensions),
            parse_numeric_or_symbol(c[6], dimensions),
            parse_numeric_or_symbol(c[7], dimensions),
        });
    }
    if (joints.size() != 6) {
        throw std::runtime_error("This proof of concept expects exactly 6 DH joints");
    }
    return joints;
}

std::vector<TagSpec> load_tag_table(const std::string& path, const std::map<std::string, double>& dimensions) {
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("Could not open AprilTag table: " + path);
    }

    std::string line;
    std::getline(f, line);
    std::vector<TagSpec> tags;
    while (std::getline(f, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> c = split_csv_line(line);
        if (c.size() != 5) {
            throw std::runtime_error("Invalid AprilTag row: " + line);
        }
        tags.push_back({
            c[0],
            std::stoi(c[1]),
            {
                parse_numeric_or_symbol(c[2], dimensions),
                parse_numeric_or_symbol(c[3], dimensions),
                parse_numeric_or_symbol(c[4], dimensions),
            },
        });
    }
    return tags;
}

std::vector<double> initial_joint_angles_deg(const std::vector<JointSpec>& joints) {
    std::vector<double> out;
    out.reserve(joints.size());
    for (const JointSpec& joint : joints) {
        out.push_back(joint.initial_deg);
    }
    return out;
}

std::vector<double> clamp_to_limits(const std::vector<JointSpec>& joints, const std::vector<double>& q_deg) {
    std::vector<double> out = q_deg;
    for (size_t i = 0; i < joints.size(); ++i) {
        out[i] = std::min(std::max(out[i], joints[i].min_deg), joints[i].max_deg);
    }
    return out;
}

std::vector<Mat4> joint_transforms(const std::vector<JointSpec>& joints, const std::vector<double>& q_deg) {
    if (joints.size() != q_deg.size()) {
        throw std::runtime_error("Joint angle count does not match DH table");
    }

    std::vector<Mat4> transforms;
    transforms.reserve(joints.size() + 1);
    Mat4 t = identity4();
    transforms.push_back(t);
    for (size_t i = 0; i < joints.size(); ++i) {
        const double theta = deg_to_rad(q_deg[i]) + joints[i].theta_offset_rad;
        t = multiply(t, dh_transform(joints[i].a_m, joints[i].alpha_rad, joints[i].d_m, theta));
        transforms.push_back(t);
    }
    return transforms;
}

std::vector<Vec3> chain_points(const std::vector<JointSpec>& joints, const std::vector<double>& q_deg) {
    const std::vector<Mat4> transforms = joint_transforms(joints, q_deg);
    std::vector<Vec3> out;
    out.reserve(transforms.size());
    for (Mat4 t : transforms) {
        out.push_back({t.m[0][3], t.m[1][3], t.m[2][3]});
    }
    return out;
}

Vec3 end_effector_position(const std::vector<JointSpec>& joints, const std::vector<double>& q_deg) {
    return chain_points(joints, q_deg).back();
}

IkResult solve_ik_position_only(
    const std::vector<JointSpec>& joints,
    Vec3 target_xyz_m,
    const std::vector<double>& q_init_deg,
    int max_iters,
    double damping,
    double tolerance_m) {
    std::vector<double> q = clamp_to_limits(joints, q_init_deg);
    for (int iter = 0; iter < max_iters; ++iter) {
        const Vec3 current = end_effector_position(joints, q);
        const Vec3 err{target_xyz_m.x - current.x, target_xyz_m.y - current.y, target_xyz_m.z - current.z};
        const double err_norm = norm3(err);
        if (err_norm <= tolerance_m) {
            return {q, true, err_norm};
        }

        const std::vector<std::vector<double>> jac = position_jacobian(joints, q);
        std::vector<std::vector<double>> lhs(3, std::vector<double>(3, 0.0));
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                for (size_t k = 0; k < q.size(); ++k) {
                    lhs[r][c] += jac[r][k] * jac[c][k];
                }
            }
            lhs[r][r] += damping * damping;
        }
        const std::vector<double> y = solve_linear_system(lhs, {err.x, err.y, err.z});
        for (size_t i = 0; i < q.size(); ++i) {
            double dq_rad = 0.0;
            for (int r = 0; r < 3; ++r) {
                dq_rad += jac[r][i] * y[r];
            }
            q[i] += rad_to_deg(dq_rad);
        }
        q = clamp_to_limits(joints, q);
    }

    const double final_error = distance(target_xyz_m, end_effector_position(joints, q));
    return {q, false, final_error};
}

std::vector<Vec3> tag_centers_from_joint_angles(
    const std::vector<JointSpec>& joints,
    const std::vector<TagSpec>& tags,
    const std::vector<double>& q_deg) {
    const std::vector<Mat4> transforms = joint_transforms(joints, q_deg);
    std::vector<Vec3> out;
    out.reserve(tags.size());
    for (const TagSpec& tag : tags) {
        const int transform_index = tag.attached_after_joint + 1;
        if (transform_index < 0 || transform_index >= static_cast<int>(transforms.size())) {
            throw std::runtime_error("AprilTag is attached to an invalid joint index");
        }
        out.push_back(transform_point(transforms[transform_index], tag.local_xyz_m));
    }
    return out;
}

TagPoseResult estimate_pose_from_tags(
    const std::vector<JointSpec>& joints,
    const std::vector<TagSpec>& tags,
    const std::vector<Vec3>& observed_centers,
    const std::vector<double>& q_init_deg,
    int max_iters,
    double damping,
    double tolerance_m) {
    if (tags.size() != observed_centers.size()) {
        throw std::runtime_error("Observed AprilTag count does not match configured tags");
    }

    std::vector<double> q = clamp_to_limits(joints, q_init_deg);
    for (int iter = 0; iter < max_iters; ++iter) {
        const std::vector<double> err = tag_error_vector(joints, tags, observed_centers, q);
        const double err_rms = rms(err);
        if (err_rms <= tolerance_m) {
            return {q, end_effector_position(joints, q), true, err_rms};
        }

        const std::vector<std::vector<double>> jac = tag_jacobian(joints, tags, observed_centers, q);
        const int dof = static_cast<int>(q.size());
        std::vector<std::vector<double>> lhs(dof, std::vector<double>(dof, 0.0));
        std::vector<double> rhs(dof, 0.0);
        for (size_t row = 0; row < jac.size(); ++row) {
            for (int c = 0; c < dof; ++c) {
                rhs[c] -= jac[row][c] * err[row];
                for (int k = 0; k < dof; ++k) {
                    lhs[c][k] += jac[row][c] * jac[row][k];
                }
            }
        }
        for (int i = 0; i < dof; ++i) {
            lhs[i][i] += damping * damping;
        }

        const std::vector<double> dq_rad = solve_linear_system(lhs, rhs);
        for (int i = 0; i < dof; ++i) {
            const double dq_deg = std::min(std::max(rad_to_deg(dq_rad[i]), -5.0), 5.0);
            q[i] += dq_deg;
        }
        q = clamp_to_limits(joints, q);
    }

    const std::vector<double> final_error = tag_error_vector(joints, tags, observed_centers, q);
    return {q, end_effector_position(joints, q), false, rms(final_error)};
}

double distance(Vec3 a, Vec3 b) {
    return norm3({a.x - b.x, a.y - b.y, a.z - b.z});
}

}  // namespace arm

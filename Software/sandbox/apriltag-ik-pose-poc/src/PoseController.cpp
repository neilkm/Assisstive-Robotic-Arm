#include "PoseController.hpp"

#include <QDir>
#include <QFile>
#include <QDebug>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

constexpr int kIkMaxIters = 180;
constexpr double kIkDamping = 0.035;
constexpr double kIkToleranceM = 0.001;
constexpr int kTagMaxIters = 240;
constexpr double kTagDamping = 0.025;
constexpr double kTagToleranceM = 0.001;
constexpr double kMotionSpeedDegPerS = 5.0;
constexpr double kAnimationFps = 10.0;

QString requireStringField(const QVariantMap& row, const QString& fieldName) {
    const QString value = row.value(fieldName).toString().trimmed();
    if (value.isEmpty()) {
        throw std::runtime_error(QStringLiteral("Missing config field: %1").arg(fieldName).toStdString());
    }
    return value;
}

QString readTextFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QStringLiteral("Could not read %1").arg(path).toStdString());
    }
    return QString::fromUtf8(file.readAll());
}

void writeTextFile(const QString& path, const QString& content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        throw std::runtime_error(QStringLiteral("Could not write %1").arg(path).toStdString());
    }
    file.write(content.toUtf8());
}

}  // namespace

namespace armui {

PoseController::PoseController(QString projectRoot, QObject* parent)
    : QObject(parent),
      projectRoot_(std::move(projectRoot)) {
    animationTimer_.setInterval(static_cast<int>(1000.0 / kAnimationFps));
    connect(&animationTimer_, &QTimer::timeout, this, &PoseController::advanceAnimation);
    reloadConfig();
}

QVariantList PoseController::chainPoints() const {
    QVariantList out;
    for (size_t i = 0; i < current_.chain_points_m.size(); ++i) {
        QVariantMap point = vecToVariant(current_.chain_points_m[i]);
        point.insert(QStringLiteral("label"), i + 1 == current_.chain_points_m.size()
                                              ? QStringLiteral("EE")
                                              : QStringLiteral("J%1").arg(i));
        out.append(point);
    }
    return out;
}

QVariantList PoseController::tagPoints() const {
    QVariantList out;
    for (size_t i = 0; i < current_.tag_centers_m.size(); ++i) {
        QVariantMap point = vecToVariant(current_.tag_centers_m[i]);
        if (i < tags_.size()) {
            point.insert(QStringLiteral("label"), QString::fromStdString(tags_[i].id));
        }
        point.insert(QStringLiteral("visible"),
                     i >= tagVisibility_.size() ? true : tagVisibility_[i]);
        out.append(point);
    }
    return out;
}

QVariantMap PoseController::targetPoint() const {
    return vecToVariant(current_.target_xyz_m);
}

QVariantMap PoseController::actualEndEffector() const {
    return vecToVariant(current_.actual_ee_xyz_m);
}

QVariantMap PoseController::tagEndEffector() const {
    return vecToVariant(current_.tag_ee_xyz_m);
}

QVariantList PoseController::jointAngles() const {
    QVariantList out;
    for (size_t i = 0; i < current_.q_deg.size(); ++i) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), i < joints_.size() ? QString::fromStdString(joints_[i].name)
                                                              : QStringLiteral("joint%1").arg(i));
        row.insert(QStringLiteral("angleDeg"), current_.q_deg[i]);
        row.insert(QStringLiteral("tagAngleDeg"), i < current_.tag_q_deg.size() ? current_.tag_q_deg[i] : 0.0);
        out.append(row);
    }
    return out;
}

QVariantList PoseController::dimensions() const {
    QVariantList out;
    for (const DimensionRow& dimension : dimensionRows_) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), dimension.name);
        row.insert(QStringLiteral("value"), dimension.value_m);
        out.append(row);
    }
    return out;
}

QVariantList PoseController::dhRows() const {
    QVariantList out;
    for (const DhRow& row : dhRows_) {
        QVariantMap map;
        map.insert(QStringLiteral("name"), row.name);
        map.insert(QStringLiteral("a_m"), row.a_m);
        map.insert(QStringLiteral("alpha_rad"), row.alpha_rad);
        map.insert(QStringLiteral("d_m"), row.d_m);
        map.insert(QStringLiteral("theta_offset_rad"), row.theta_offset_rad);
        map.insert(QStringLiteral("initial_deg"), row.initial_deg);
        map.insert(QStringLiteral("min_deg"), row.min_deg);
        map.insert(QStringLiteral("max_deg"), row.max_deg);
        out.append(map);
    }
    return out;
}

double PoseController::axisLimit() const {
    double limit = 0.0;
    for (int i = 0; i < 6; ++i) {
        const auto it = dimensionTable_.find(QStringLiteral("L%1").arg(i).toStdString());
        if (it != dimensionTable_.end()) {
            limit += it->second;
        }
    }
    return std::max(1.0, limit);
}

bool PoseController::ikConverged() const {
    return current_.ik_converged;
}

bool PoseController::tagConverged() const {
    return current_.tag_converged;
}

double PoseController::ikErrorM() const {
    return current_.ik_error_m;
}

double PoseController::tagRmsErrorM() const {
    return current_.tag_rms_error_m;
}

double PoseController::endEffectorCompareErrorM() const {
    return current_.ee_compare_error_m;
}

double PoseController::targetActualErrorM() const {
    return current_.target_actual_error_m;
}

double PoseController::targetCalculatedErrorM() const {
    return current_.target_calculated_error_m;
}

double PoseController::endEffectorRotationDeg() const {
    if (current_.q_deg.size() <= 5) {
        return 0.0;
    }
    return current_.q_deg[5];
}

double PoseController::endEffectorRotationMinDeg() const {
    if (joints_.size() <= 5) {
        return -180.0;
    }
    return joints_[5].min_deg;
}

double PoseController::endEffectorRotationMaxDeg() const {
    if (joints_.size() <= 5) {
        return 180.0;
    }
    return joints_[5].max_deg;
}

double PoseController::approachAngleDeg() const {
    return approachAngleDeg_;
}

QString PoseController::solverWarning() const {
    return current_.solver_warning;
}

QVariantList PoseController::tagVisibility() const {
    QVariantList out;
    for (size_t i = 0; i < tagVisibility_.size(); ++i) {
        QVariantMap row;
        row.insert(QStringLiteral("index"), static_cast<int>(i));
        row.insert(QStringLiteral("id"), i < tags_.size() ? QString::fromStdString(tags_[i].id)
                                                          : QStringLiteral("tag%1").arg(i));
        row.insert(QStringLiteral("visible"), static_cast<bool>(tagVisibility_[i]));
        out.append(row);
    }
    return out;
}

double PoseController::motionSpeedDegPerS() const {
    return kMotionSpeedDegPerS;
}

bool PoseController::moving() const {
    return animationTimer_.isActive();
}

bool PoseController::cubeTestRunning() const {
    return cubeTestRunning_;
}

int PoseController::cubeTestStep() const {
    return cubeTestStep_;
}

int PoseController::cubeTestStepCount() const {
    return cubeTestStepCount_;
}

QString PoseController::message() const {
    return message_;
}

void PoseController::moveToTarget(double x_m, double y_m, double z_m) {
    if (joints_.empty()) {
        setPose(current_, QStringLiteral("No robot config is loaded."));
        return;
    }

    try {
        cubeTestRunning_ = false;
        cubeTestGoals_.clear();
        animationTimer_.stop();
        const arm::Vec3 target{x_m, y_m, z_m};
        std::vector<double> q0 = arm_geometry::InitialJointAngles(geometry_);
        std::vector<bool> fixedJoints(q0.size(), false);
        if (q0.size() > 5) {
            q0[5] = approachAngleDeg_;
            fixedJoints[5] = true;
        }
        std::vector<double> solved;
        double error_m = 0.0;
        const bool converged = arm_fk::SolveIkPositionOnlyWithFixedJoints(
            geometry_, {x_m, y_m, z_m}, q0, fixedJoints, kIkMaxIters,
            kIkDamping, kIkToleranceM, &solved, &error_m);
        PoseState goal = poseFromAngles(solved, target, converged, error_m);
        qInfo() << "Move target" << x_m << y_m << z_m << "approach joint5 deg"
                << approachAngleDeg_ << "ik_converged" << converged
                << "ik_error_m" << error_m;
        startAnimationTo(goal, QStringLiteral("Moving to target."), QStringLiteral("Move complete."));
    } catch (const std::exception& e) {
        message_ = QStringLiteral("Move failed: %1").arg(QString::fromUtf8(e.what()));
        emit poseChanged();
    }
}

void PoseController::setEndEffectorRotation(double joint5_deg) {
    if (joints_.size() <= 5 || current_.q_deg.size() <= 5) {
        setPose(current_, QStringLiteral("Joint5 is not available in the current config."));
        return;
    }

    try {
        cubeTestRunning_ = false;
        cubeTestGoals_.clear();
        cubeTestStep_ = 0;
        cubeTestStepCount_ = 0;
        animationTimer_.stop();

        std::vector<double> q = current_.q_deg;
        q[5] = std::min(std::max(joint5_deg, joints_[5].min_deg), joints_[5].max_deg);
        approachAngleDeg_ = q[5];
        setPose(poseFromAngles(q, current_.target_xyz_m, current_.ik_converged,
                               current_.ik_error_m),
                QStringLiteral("End effector rotation set from joint5."));
        qInfo() << "Joint5 end-effector rotation set to" << q[5] << "deg";
    } catch (const std::exception& e) {
        message_ = QStringLiteral("Rotation update failed: %1").arg(QString::fromUtf8(e.what()));
        emit poseChanged();
    }
}

void PoseController::nudgeEndEffectorRotation(double delta_deg) {
    setEndEffectorRotation(endEffectorRotationDeg() + delta_deg);
}

void PoseController::setApproachAngle(double approach_deg) {
    if (joints_.size() > 5) {
        approachAngleDeg_ =
            std::min(std::max(approach_deg, joints_[5].min_deg), joints_[5].max_deg);
    } else {
        approachAngleDeg_ = approach_deg;
    }
    message_ = QStringLiteral("Approach angle will be applied on the next Move.");
    qInfo() << "Approach angle set to" << approachAngleDeg_ << "deg";
    emit poseChanged();
}

void PoseController::setAprilTagVisible(int tag_index, bool is_visible) {
    if (tag_index < 0 || tag_index >= static_cast<int>(tagVisibility_.size())) {
        message_ = QStringLiteral("Invalid AprilTag visibility index.");
        emit poseChanged();
        return;
    }
    tagVisibility_[static_cast<size_t>(tag_index)] = is_visible;
    qInfo() << "AprilTag visibility changed" << tag_index << is_visible;
    setPose(poseFromAngles(current_.q_deg, current_.target_xyz_m,
                           current_.ik_converged, current_.ik_error_m),
            QStringLiteral("AprilTag visibility updated."));
}

void PoseController::runCubeTest() {
    if (joints_.empty()) {
        setPose(current_, QStringLiteral("No robot config is loaded."));
        return;
    }

    try {
        animationTimer_.stop();
        cubeTestGoals_.clear();
        cubeTestStep_ = 0;

        const std::vector<double> q_zero(joints_.size(), 0.0);
        const std::vector<arm::Vec3> zeroPoints = arm::chain_points(joints_, q_zero);
        if (zeroPoints.empty()) {
            throw std::runtime_error("Robot chain has no base joint point");
        }

        const arm::Vec3 base = zeroPoints.front();
        constexpr double sideLength = 12.0;
        constexpr double halfSide = sideLength / 2.0;
        const std::vector<arm::Vec3> targets = {
            {base.x - halfSide, base.y - halfSide, base.z},
            {base.x + halfSide, base.y - halfSide, base.z},
            {base.x + halfSide, base.y + halfSide, base.z},
            {base.x - halfSide, base.y + halfSide, base.z},
            {base.x - halfSide, base.y - halfSide, base.z + sideLength},
            {base.x + halfSide, base.y - halfSide, base.z + sideLength},
            {base.x + halfSide, base.y + halfSide, base.z + sideLength},
            {base.x - halfSide, base.y + halfSide, base.z + sideLength},
        };

        std::vector<double> q0 = arm_geometry::InitialJointAngles(geometry_);
        std::vector<bool> fixedJoints(q0.size(), false);
        if (q0.size() > 5) {
            q0[5] = approachAngleDeg_;
            fixedJoints[5] = true;
        }
        for (arm::Vec3 target : targets) {
            std::vector<double> solved;
            double error_m = 0.0;
            const bool converged = arm_fk::SolveIkPositionOnlyWithFixedJoints(
                geometry_, {target.x, target.y, target.z}, q0, fixedJoints,
                kIkMaxIters, kIkDamping, kIkToleranceM, &solved, &error_m);
            cubeTestGoals_.push_back(
                poseFromAngles(solved, target, converged, error_m));
        }
        const arm::Vec3 zeroTarget = arm::end_effector_position(joints_, q_zero);
        cubeTestGoals_.push_back(poseFromAngles(q_zero, zeroTarget, true, 0.0));

        cubeTestRunning_ = true;
        cubeTestStepCount_ = static_cast<int>(cubeTestGoals_.size());
        qInfo() << "Cube test started with" << cubeTestStepCount_ << "steps";
        runNextCubeTestStep();
    } catch (const std::exception& e) {
        cubeTestRunning_ = false;
        cubeTestGoals_.clear();
        cubeTestStep_ = 0;
        cubeTestStepCount_ = 0;
        message_ = QStringLiteral("Cube test failed: %1").arg(QString::fromUtf8(e.what()));
        emit poseChanged();
    }
}

void PoseController::resetZero() {
    if (joints_.empty()) {
        setPose(current_, QStringLiteral("No robot config is loaded."));
        return;
    }

    try {
        cubeTestRunning_ = false;
        cubeTestGoals_.clear();
        cubeTestStep_ = 0;
        cubeTestStepCount_ = 0;
        animationTimer_.stop();
        const std::vector<double> q_zero(joints_.size(), 0.0);
        const arm::Vec3 target = arm::end_effector_position(joints_, q_zero);
        setPose(poseFromAngles(q_zero, target, true, 0.0), QStringLiteral("Zero pose loaded."));
    } catch (const std::exception& e) {
        message_ = QStringLiteral("Reset failed: %1").arg(QString::fromUtf8(e.what()));
        emit poseChanged();
    }
}

bool PoseController::reloadConfig() {
    try {
        cubeTestRunning_ = false;
        cubeTestGoals_.clear();
        cubeTestStep_ = 0;
        cubeTestStepCount_ = 0;
        animationTimer_.stop();
        loadConfigFromDisk();
        emit configChanged();
        resetZero();
        message_ = QStringLiteral("Config loaded from CSV.");
        emit poseChanged();
        return true;
    } catch (const std::exception& e) {
        message_ = QStringLiteral("Config load failed: %1").arg(QString::fromUtf8(e.what()));
        emit poseChanged();
        return false;
    }
}

bool PoseController::saveConfig(const QVariantList& dimensions, const QVariantList& dhRows) {
    const QString geometryPath = projectFile(QStringLiteral("configs/arm_geometry.csv"));
    QString oldGeometry;

    try {
        oldGeometry = readTextFile(geometryPath);

        QString geometryContent = QStringLiteral(
            "record_type,name,value,a_m,alpha_rad,d_m,theta_offset_rad,initial_deg,min_deg,max_deg,"
            "attached_after_joint,local_x_m,local_y_m,local_z_m,local_roll_rad,local_pitch_rad,local_yaw_rad\n");
        for (const QVariant& value : dimensions) {
            const QVariantMap row = value.toMap();
            const QString name = requireStringField(row, QStringLiteral("name"));
            const QString rawValue = requireStringField(row, QStringLiteral("value"));
            bool ok = false;
            const double parsed = rawValue.toDouble(&ok);
            if (!ok || parsed <= 0.0) {
                throw std::runtime_error(QStringLiteral("Invalid dimension value for %1").arg(name).toStdString());
            }
            geometryContent += QStringLiteral("dimension,%1,%2,,,,,,,,,,,,,,\n")
                                   .arg(name, QString::number(parsed, 'f', 6));
        }

        if (dhRows.size() != 6) {
            throw std::runtime_error("DH table must contain exactly 6 rows");
        }
        for (const QVariant& value : dhRows) {
            const QVariantMap row = value.toMap();
            geometryContent += QStringLiteral("joint,%1,,%2,%3,%4,%5,%6,%7,%8,,,,,,,\n")
                                   .arg(requireStringField(row, QStringLiteral("name")),
                                        requireStringField(row, QStringLiteral("a_m")),
                                        requireStringField(row, QStringLiteral("alpha_rad")),
                                        requireStringField(row, QStringLiteral("d_m")),
                                        requireStringField(row, QStringLiteral("theta_offset_rad")),
                                        requireStringField(row, QStringLiteral("initial_deg")),
                                        requireStringField(row, QStringLiteral("min_deg")),
                                        requireStringField(row, QStringLiteral("max_deg")));
        }

        for (const arm_geometry::AprilTagGeometry& tag : geometry_.april_tags) {
            geometryContent += QStringLiteral("apriltag,%1,,,,,,,,,%2,%3,%4,%5,%6,%7,%8\n")
                                   .arg(QString::fromStdString(tag.id),
                                        QString::number(tag.attached_after_joint),
                                        QString::number(tag.local_pose.position_m.x, 'g', 12),
                                        QString::number(tag.local_pose.position_m.y, 'g', 12),
                                        QString::number(tag.local_pose.position_m.z, 'g', 12),
                                        QString::number(tag.local_pose.euler_rad.roll_rad, 'g', 12),
                                        QString::number(tag.local_pose.euler_rad.pitch_rad, 'g', 12),
                                        QString::number(tag.local_pose.euler_rad.yaw_rad, 'g', 12));
        }

        writeTextFile(geometryPath, geometryContent);
        loadConfigFromDisk();
        emit configChanged();
        resetZero();
        message_ = QStringLiteral("Config saved to CSV.");
        emit poseChanged();
        return true;
    } catch (const std::exception& e) {
        if (!oldGeometry.isEmpty()) {
            writeTextFile(geometryPath, oldGeometry);
        }
        message_ = QStringLiteral("Config save failed: %1").arg(QString::fromUtf8(e.what()));
        emit poseChanged();
        return false;
    }
}

QString PoseController::projectFile(const QString& relativePath) const {
    return QDir(projectRoot_).filePath(relativePath);
}

void PoseController::loadConfigFromDisk() {
    geometry_ = arm_geometry::LoadArmGeometry(projectFile(QStringLiteral("configs/arm_geometry.csv")).toStdString());
    dimensionTable_ = geometry_.dimensions;

    dimensionRows_.clear();
    const std::vector<QString> dimensionOrder = {
        QStringLiteral("L0"), QStringLiteral("L1"), QStringLiteral("L2"),
        QStringLiteral("L3"), QStringLiteral("L4"), QStringLiteral("L5"),
        QStringLiteral("W0"), QStringLiteral("W1"), QStringLiteral("W2"),
        QStringLiteral("G0"),
    };
    for (const QString& name : dimensionOrder) {
        const auto it = dimensionTable_.find(name.toStdString());
        if (it != dimensionTable_.end()) {
            dimensionRows_.push_back({name, it->second});
        }
    }

    dhRows_.clear();
    joints_.clear();
    for (const arm_geometry::JointGeometry& joint : geometry_.joints) {
        dhRows_.push_back({
            QString::fromStdString(joint.name),
            QString::number(joint.a_m, 'g', 12),
            QString::number(joint.alpha_rad, 'g', 12),
            QString::number(joint.d_m, 'g', 12),
            QString::number(joint.theta_offset_rad, 'g', 12),
            QString::number(joint.initial_deg, 'g', 12),
            QString::number(joint.min_deg, 'g', 12),
            QString::number(joint.max_deg, 'g', 12),
        });
        joints_.push_back({
            joint.name,
            joint.a_m,
            joint.alpha_rad,
            joint.d_m,
            joint.theta_offset_rad,
            joint.initial_deg,
            joint.min_deg,
            joint.max_deg,
        });
    }

    tags_.clear();
    for (const arm_geometry::AprilTagGeometry& tag : geometry_.april_tags) {
        tags_.push_back({
            tag.id,
            tag.attached_after_joint,
            {tag.local_pose.position_m.x, tag.local_pose.position_m.y,
             tag.local_pose.position_m.z},
        });
    }
    if (tagVisibility_.size() != geometry_.april_tags.size()) {
        tagVisibility_.assign(geometry_.april_tags.size(), true);
    }
}

PoseController::PoseState PoseController::poseFromAngles(
    const std::vector<double>& q_deg,
    arm::Vec3 target_xyz_m,
    bool ik_converged,
    double ik_error_m) const {
    PoseState state;
    state.q_deg = arm::clamp_to_limits(joints_, q_deg);
    state.chain_points_m = arm::chain_points(joints_, state.q_deg);
    state.tag_centers_m = arm::tag_centers_from_joint_angles(joints_, tags_, state.q_deg);
    state.target_xyz_m = target_xyz_m;
    state.actual_ee_xyz_m = arm::end_effector_position(joints_, state.q_deg);
    state.ik_converged = ik_converged;
    state.ik_error_m = ik_error_m;

    arm_fk::GeneratedRobotState generated = arm_fk::GenerateRobotState(geometry_, state.q_deg);
    std::vector<arm_pose_estimator::AprilTagStatus> statuses;
    for (size_t i = 0; i < geometry_.april_tags.size(); ++i) {
        const bool is_visible = i >= tagVisibility_.size() ? true : tagVisibility_[i];
        statuses.push_back({geometry_.april_tags[i].id, generated.april_tag_poses[i], is_visible});
    }
    const arm_pose_estimator::EstimateResult estimate =
        arm_pose_estimator::EstimateEndEffectorPose(geometry_, statuses);
    state.tag_q_deg = state.q_deg;
    state.tag_ee_xyz_m = {
        estimate.end_effector_pose.position_m.x,
        estimate.end_effector_pose.position_m.y,
        estimate.end_effector_pose.position_m.z,
    };
    state.tag_converged = estimate.converged;
    state.tag_rms_error_m = estimate.rms_error;
    state.ee_compare_error_m = arm::distance(state.actual_ee_xyz_m, state.tag_ee_xyz_m);
    state.target_actual_error_m = arm::distance(state.target_xyz_m, state.actual_ee_xyz_m);
    state.target_calculated_error_m = arm::distance(state.target_xyz_m, state.tag_ee_xyz_m);
    if (!state.ik_converged) {
        state.solver_warning += QStringLiteral("IK did not converge. ");
    }
    if (!state.tag_converged) {
        state.solver_warning += QStringLiteral("AprilTag pose estimate did not converge. ");
    }
    if (!QString::fromStdString(estimate.warning).isEmpty()) {
        state.solver_warning += QString::fromStdString(estimate.warning);
    }
    return state;
}

void PoseController::setPose(PoseState state, QString message) {
    current_ = std::move(state);
    message_ = std::move(message);
    emit poseChanged();
}

void PoseController::startAnimationTo(PoseState goalState, QString runningMessage, QString doneMessage) {
    if (current_.q_deg.size() != goalState.q_deg.size()) {
        setPose(std::move(goalState), std::move(doneMessage));
        return;
    }

    double maxDeltaDeg = 0.0;
    for (size_t i = 0; i < current_.q_deg.size(); ++i) {
        maxDeltaDeg = std::max(maxDeltaDeg, std::fabs(goalState.q_deg[i] - current_.q_deg[i]));
    }

    if (maxDeltaDeg < 1e-9) {
        setPose(std::move(goalState), std::move(doneMessage));
        if (cubeTestRunning_) {
            QTimer::singleShot(0, this, &PoseController::runNextCubeTestStep);
        }
        return;
    }

    animationStart_ = current_;
    animationGoal_ = std::move(goalState);
    animationFrameIndex_ = 0;
    animationFrameCount_ = std::max(2, static_cast<int>(std::ceil((maxDeltaDeg / kMotionSpeedDegPerS) * kAnimationFps)));
    animationRunningMessage_ = std::move(runningMessage);
    animationDoneMessage_ = std::move(doneMessage);
    message_ = animationRunningMessage_;
    animationTimer_.start();
    emit poseChanged();
}

void PoseController::advanceAnimation() {
    ++animationFrameIndex_;
    if (animationFrameIndex_ >= animationFrameCount_) {
        animationTimer_.stop();
        current_ = animationGoal_;
        message_ = animationGoal_.ik_converged && animationGoal_.tag_converged
                       ? animationDoneMessage_
                       : animationDoneMessage_ + QStringLiteral(" Solver warning.");
        emit poseChanged();
        if (cubeTestRunning_) {
            runNextCubeTestStep();
        }
        return;
    }

    const double t = static_cast<double>(animationFrameIndex_) / static_cast<double>(animationFrameCount_);
    std::vector<double> q(animationStart_.q_deg.size(), 0.0);
    for (size_t i = 0; i < q.size(); ++i) {
        q[i] = animationStart_.q_deg[i] + t * (animationGoal_.q_deg[i] - animationStart_.q_deg[i]);
    }

    PoseState frame = poseFromAngles(
        q,
        animationGoal_.target_xyz_m,
        animationGoal_.ik_converged,
        animationGoal_.ik_error_m);
    setPose(std::move(frame), animationRunningMessage_);
}

void PoseController::runNextCubeTestStep() {
    if (!cubeTestRunning_) {
        return;
    }

    if (cubeTestStep_ >= static_cast<int>(cubeTestGoals_.size())) {
        cubeTestRunning_ = false;
        cubeTestGoals_.clear();
        cubeTestStep_ = 0;
        cubeTestStepCount_ = 0;
        message_ = QStringLiteral("Cube test complete. Returned to zero joint angles.");
        emit poseChanged();
        return;
    }

    const bool returningToZero = cubeTestStep_ + 1 == static_cast<int>(cubeTestGoals_.size());
    ++cubeTestStep_;
    const QString runningMessage = returningToZero
                                       ? QStringLiteral("Cube test: returning to zero joint angles.")
                                       : QStringLiteral("Cube test: moving to vertex %1 of 8.").arg(cubeTestStep_);
    const QString doneMessage = returningToZero
                                    ? QStringLiteral("Cube test: zero joint pose reached.")
                                    : QStringLiteral("Cube test: vertex %1 reached.").arg(cubeTestStep_);
    startAnimationTo(cubeTestGoals_[static_cast<size_t>(cubeTestStep_ - 1)], runningMessage, doneMessage);
}

QVariantMap PoseController::vecToVariant(arm::Vec3 value) const {
    QVariantMap out;
    out.insert(QStringLiteral("x"), value.x);
    out.insert(QStringLiteral("y"), value.y);
    out.insert(QStringLiteral("z"), value.z);
    return out;
}

QVariantList PoseController::vecListToVariant(const std::vector<arm::Vec3>& values) const {
    QVariantList out;
    for (arm::Vec3 value : values) {
        out.append(vecToVariant(value));
    }
    return out;
}

QVariantList PoseController::numberListToVariant(const std::vector<double>& values) const {
    QVariantList out;
    for (double value : values) {
        out.append(value);
    }
    return out;
}

}  // namespace armui

#include "PoseController.hpp"

#include <QDir>
#include <QFile>
#include <QTextStream>

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
        const std::vector<double> q0 = arm::initial_joint_angles_deg(joints_);
        const arm::IkResult ik = arm::solve_ik_position_only(
            joints_, target, q0, kIkMaxIters, kIkDamping, kIkToleranceM);
        PoseState goal = poseFromAngles(ik.q_deg, target, ik.converged, ik.error_m, true);
        startAnimationTo(goal, QStringLiteral("Moving to target."), QStringLiteral("Move complete."));
    } catch (const std::exception& e) {
        message_ = QStringLiteral("Move failed: %1").arg(QString::fromUtf8(e.what()));
        emit poseChanged();
    }
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

        const std::vector<double> q0 = arm::initial_joint_angles_deg(joints_);
        for (arm::Vec3 target : targets) {
            const arm::IkResult ik = arm::solve_ik_position_only(
                joints_, target, q0, kIkMaxIters, kIkDamping, kIkToleranceM);
            cubeTestGoals_.push_back(poseFromAngles(ik.q_deg, target, ik.converged, ik.error_m, true));
        }
        const arm::Vec3 zeroTarget = arm::end_effector_position(joints_, q_zero);
        cubeTestGoals_.push_back(poseFromAngles(q_zero, zeroTarget, true, 0.0, true));

        cubeTestRunning_ = true;
        cubeTestStepCount_ = static_cast<int>(cubeTestGoals_.size());
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
        setPose(poseFromAngles(q_zero, target, true, 0.0, true), QStringLiteral("Zero pose loaded."));
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
    const QString dimensionsPath = projectFile(QStringLiteral("configs/robot_dimensions.csv"));
    const QString dhPath = projectFile(QStringLiteral("configs/dh_table.csv"));
    QString oldDimensions;
    QString oldDh;

    try {
        oldDimensions = readTextFile(dimensionsPath);
        oldDh = readTextFile(dhPath);

        QString dimensionsContent = QStringLiteral("name,value_m\n");
        for (const QVariant& value : dimensions) {
            const QVariantMap row = value.toMap();
            const QString name = requireStringField(row, QStringLiteral("name"));
            const QString rawValue = requireStringField(row, QStringLiteral("value"));
            bool ok = false;
            const double parsed = rawValue.toDouble(&ok);
            if (!ok || parsed <= 0.0) {
                throw std::runtime_error(QStringLiteral("Invalid dimension value for %1").arg(name).toStdString());
            }
            dimensionsContent += QStringLiteral("%1,%2\n").arg(name, QString::number(parsed, 'f', 6));
        }

        if (dhRows.size() != 6) {
            throw std::runtime_error("DH table must contain exactly 6 rows");
        }
        QString dhContent = QStringLiteral("name,a_m,alpha_rad,d_m,theta_offset_rad,initial_deg,min_deg,max_deg\n");
        for (const QVariant& value : dhRows) {
            const QVariantMap row = value.toMap();
            dhContent += QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8\n")
                             .arg(requireStringField(row, QStringLiteral("name")),
                                  requireStringField(row, QStringLiteral("a_m")),
                                  requireStringField(row, QStringLiteral("alpha_rad")),
                                  requireStringField(row, QStringLiteral("d_m")),
                                  requireStringField(row, QStringLiteral("theta_offset_rad")),
                                  requireStringField(row, QStringLiteral("initial_deg")),
                                  requireStringField(row, QStringLiteral("min_deg")),
                                  requireStringField(row, QStringLiteral("max_deg")));
        }

        writeTextFile(dimensionsPath, dimensionsContent);
        writeTextFile(dhPath, dhContent);
        loadConfigFromDisk();
        emit configChanged();
        resetZero();
        message_ = QStringLiteral("Config saved to CSV.");
        emit poseChanged();
        return true;
    } catch (const std::exception& e) {
        if (!oldDimensions.isEmpty()) {
            writeTextFile(dimensionsPath, oldDimensions);
        }
        if (!oldDh.isEmpty()) {
            writeTextFile(dhPath, oldDh);
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
    dimensionRows_ = readDimensionRows();
    dhRows_ = readDhRows();
    dimensionTable_ = arm::load_dimension_table(projectFile(QStringLiteral("configs/robot_dimensions.csv")).toStdString());
    joints_ = arm::load_dh_table(projectFile(QStringLiteral("configs/dh_table.csv")).toStdString(), dimensionTable_);
    tags_ = arm::load_tag_table(projectFile(QStringLiteral("configs/apriltags.csv")).toStdString(), dimensionTable_);
}

std::vector<PoseController::DimensionRow> PoseController::readDimensionRows() const {
    QFile file(projectFile(QStringLiteral("configs/robot_dimensions.csv")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Could not open robot_dimensions.csv");
    }

    QTextStream in(&file);
    in.readLine();
    std::vector<DimensionRow> rows;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList cols = line.split(QLatin1Char(','), Qt::KeepEmptyParts);
        if (cols.size() != 2) {
            throw std::runtime_error(QStringLiteral("Invalid dimension row: %1").arg(line).toStdString());
        }
        bool ok = false;
        const double value = cols[1].trimmed().toDouble(&ok);
        if (!ok) {
            throw std::runtime_error(QStringLiteral("Invalid dimension value: %1").arg(line).toStdString());
        }
        rows.push_back({cols[0].trimmed(), value});
    }
    return rows;
}

std::vector<PoseController::DhRow> PoseController::readDhRows() const {
    QFile file(projectFile(QStringLiteral("configs/dh_table.csv")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Could not open dh_table.csv");
    }

    QTextStream in(&file);
    in.readLine();
    std::vector<DhRow> rows;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList cols = line.split(QLatin1Char(','), Qt::KeepEmptyParts);
        if (cols.size() != 8) {
            throw std::runtime_error(QStringLiteral("Invalid DH row: %1").arg(line).toStdString());
        }
        rows.push_back({
            cols[0].trimmed(),
            cols[1].trimmed(),
            cols[2].trimmed(),
            cols[3].trimmed(),
            cols[4].trimmed(),
            cols[5].trimmed(),
            cols[6].trimmed(),
            cols[7].trimmed(),
        });
    }
    return rows;
}

PoseController::PoseState PoseController::poseFromAngles(
    const std::vector<double>& q_deg,
    arm::Vec3 target_xyz_m,
    bool ik_converged,
    double ik_error_m,
    bool estimateTagsFromInitialAngles) const {
    PoseState state;
    state.q_deg = arm::clamp_to_limits(joints_, q_deg);
    state.chain_points_m = arm::chain_points(joints_, state.q_deg);
    state.tag_centers_m = arm::tag_centers_from_joint_angles(joints_, tags_, state.q_deg);
    state.target_xyz_m = target_xyz_m;
    state.actual_ee_xyz_m = arm::end_effector_position(joints_, state.q_deg);
    state.ik_converged = ik_converged;
    state.ik_error_m = ik_error_m;

    const std::vector<double> tagInitialAngles =
        estimateTagsFromInitialAngles ? arm::initial_joint_angles_deg(joints_) : state.q_deg;
    const arm::TagPoseResult tagPose = arm::estimate_pose_from_tags(
        joints_, tags_, state.tag_centers_m, tagInitialAngles, kTagMaxIters, kTagDamping, kTagToleranceM);
    state.tag_q_deg = tagPose.q_deg;
    state.tag_ee_xyz_m = tagPose.end_effector_xyz_m;
    state.tag_converged = tagPose.converged;
    state.tag_rms_error_m = tagPose.rms_error_m;
    state.ee_compare_error_m = arm::distance(state.actual_ee_xyz_m, state.tag_ee_xyz_m);
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
        animationGoal_.ik_error_m,
        false);
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

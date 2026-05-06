#pragma once

#include "kinematics.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <map>
#include <string>
#include <vector>

namespace armui {

class PoseController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList chainPoints READ chainPoints NOTIFY poseChanged)
    Q_PROPERTY(QVariantList tagPoints READ tagPoints NOTIFY poseChanged)
    Q_PROPERTY(QVariantMap targetPoint READ targetPoint NOTIFY poseChanged)
    Q_PROPERTY(QVariantMap actualEndEffector READ actualEndEffector NOTIFY poseChanged)
    Q_PROPERTY(QVariantMap tagEndEffector READ tagEndEffector NOTIFY poseChanged)
    Q_PROPERTY(QVariantList jointAngles READ jointAngles NOTIFY poseChanged)
    Q_PROPERTY(QVariantList dimensions READ dimensions NOTIFY configChanged)
    Q_PROPERTY(QVariantList dhRows READ dhRows NOTIFY configChanged)
    Q_PROPERTY(double axisLimit READ axisLimit NOTIFY configChanged)
    Q_PROPERTY(bool ikConverged READ ikConverged NOTIFY poseChanged)
    Q_PROPERTY(bool tagConverged READ tagConverged NOTIFY poseChanged)
    Q_PROPERTY(double ikErrorM READ ikErrorM NOTIFY poseChanged)
    Q_PROPERTY(double tagRmsErrorM READ tagRmsErrorM NOTIFY poseChanged)
    Q_PROPERTY(double endEffectorCompareErrorM READ endEffectorCompareErrorM NOTIFY poseChanged)
    Q_PROPERTY(double motionSpeedDegPerS READ motionSpeedDegPerS CONSTANT)
    Q_PROPERTY(bool moving READ moving NOTIFY poseChanged)
    Q_PROPERTY(bool cubeTestRunning READ cubeTestRunning NOTIFY poseChanged)
    Q_PROPERTY(int cubeTestStep READ cubeTestStep NOTIFY poseChanged)
    Q_PROPERTY(int cubeTestStepCount READ cubeTestStepCount NOTIFY poseChanged)
    Q_PROPERTY(QString message READ message NOTIFY poseChanged)

public:
    explicit PoseController(QString projectRoot, QObject* parent = nullptr);

    [[nodiscard]] QVariantList chainPoints() const;
    [[nodiscard]] QVariantList tagPoints() const;
    [[nodiscard]] QVariantMap targetPoint() const;
    [[nodiscard]] QVariantMap actualEndEffector() const;
    [[nodiscard]] QVariantMap tagEndEffector() const;
    [[nodiscard]] QVariantList jointAngles() const;
    [[nodiscard]] QVariantList dimensions() const;
    [[nodiscard]] QVariantList dhRows() const;
    [[nodiscard]] double axisLimit() const;
    [[nodiscard]] bool ikConverged() const;
    [[nodiscard]] bool tagConverged() const;
    [[nodiscard]] double ikErrorM() const;
    [[nodiscard]] double tagRmsErrorM() const;
    [[nodiscard]] double endEffectorCompareErrorM() const;
    [[nodiscard]] double motionSpeedDegPerS() const;
    [[nodiscard]] bool moving() const;
    [[nodiscard]] bool cubeTestRunning() const;
    [[nodiscard]] int cubeTestStep() const;
    [[nodiscard]] int cubeTestStepCount() const;
    [[nodiscard]] QString message() const;

    Q_INVOKABLE void moveToTarget(double x_m, double y_m, double z_m);
    Q_INVOKABLE void runCubeTest();
    Q_INVOKABLE void resetZero();
    Q_INVOKABLE bool reloadConfig();
    Q_INVOKABLE bool saveConfig(const QVariantList& dimensions, const QVariantList& dhRows);

signals:
    void poseChanged();
    void configChanged();

private:
    struct DimensionRow {
        QString name;
        double value_m;
    };

    struct DhRow {
        QString name;
        QString a_m;
        QString alpha_rad;
        QString d_m;
        QString theta_offset_rad;
        QString initial_deg;
        QString min_deg;
        QString max_deg;
    };

    struct PoseState {
        std::vector<double> q_deg;
        std::vector<double> tag_q_deg;
        std::vector<arm::Vec3> chain_points_m;
        std::vector<arm::Vec3> tag_centers_m;
        arm::Vec3 target_xyz_m{0.0, 0.0, 0.0};
        arm::Vec3 actual_ee_xyz_m{0.0, 0.0, 0.0};
        arm::Vec3 tag_ee_xyz_m{0.0, 0.0, 0.0};
        bool ik_converged = false;
        bool tag_converged = false;
        double ik_error_m = 0.0;
        double tag_rms_error_m = 0.0;
        double ee_compare_error_m = 0.0;
    };

    [[nodiscard]] QString projectFile(const QString& relativePath) const;
    void loadConfigFromDisk();
    [[nodiscard]] std::vector<DimensionRow> readDimensionRows() const;
    [[nodiscard]] std::vector<DhRow> readDhRows() const;
    [[nodiscard]] PoseState poseFromAngles(
        const std::vector<double>& q_deg,
        arm::Vec3 target_xyz_m,
        bool ik_converged,
        double ik_error_m,
        bool estimateTagsFromInitialAngles) const;
    void setPose(PoseState state, QString message);
    void startAnimationTo(PoseState goalState, QString runningMessage, QString doneMessage);
    void advanceAnimation();
    void runNextCubeTestStep();
    [[nodiscard]] QVariantMap vecToVariant(arm::Vec3 value) const;
    [[nodiscard]] QVariantList vecListToVariant(const std::vector<arm::Vec3>& values) const;
    [[nodiscard]] QVariantList numberListToVariant(const std::vector<double>& values) const;

    QString projectRoot_;
    std::map<std::string, double> dimensionTable_;
    std::vector<DimensionRow> dimensionRows_;
    std::vector<DhRow> dhRows_;
    std::vector<arm::JointSpec> joints_;
    std::vector<arm::TagSpec> tags_;
    PoseState current_;
    PoseState animationStart_;
    PoseState animationGoal_;
    QTimer animationTimer_;
    int animationFrameIndex_ = 0;
    int animationFrameCount_ = 0;
    std::vector<PoseState> cubeTestGoals_;
    bool cubeTestRunning_ = false;
    int cubeTestStep_ = 0;
    int cubeTestStepCount_ = 0;
    QString animationRunningMessage_;
    QString animationDoneMessage_;
    QString message_;
};

}  // namespace armui

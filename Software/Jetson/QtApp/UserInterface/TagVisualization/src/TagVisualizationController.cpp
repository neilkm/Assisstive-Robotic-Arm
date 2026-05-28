#include "TagVisualizationUi/TagVisualizationController.h"

#include <QSize>
#include <QVariantMap>
#include <array>
#include <cmath>
#include <optional>
#include <set>

namespace tagvisualizationui {
namespace {

using jetsonqt::objectdetection::AprilTagPose;
using jetsonqt::objectdetection::EulerAnglesDegrees;
using Matrix3 = std::array<double, 9>;
using Vector3 = std::array<double, 3>;

// OpenCV's tag-local Y/Z directions are converted so the home tag lies in the
// XY plane and +Z points up out of the printed tag surface.
constexpr Matrix3 kOpenCvTagToFlatHomeBasis = {1.0, 0.0, 0.0, 0.0, -1.0,
                                               0.0, 0.0, 0.0, -1.0};
constexpr int kPollingIntervalMs = 50;
constexpr std::size_t kPoseAverageFrameCount = 50;
constexpr int kRgbChannelCount = 3;
constexpr int kFirstFrameRevision = 1;
constexpr double kSingularEulerThreshold = 1e-6;
constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;

QVariantMap toTagPoseMap(const AprilTagPose& pose) {
  QVariantMap tag;
  tag.insert(QStringLiteral("id"), pose.id);
  tag.insert(QStringLiteral("x"), pose.position.x);
  tag.insert(QStringLiteral("y"), pose.position.y);
  tag.insert(QStringLiteral("z"), pose.position.z);
  tag.insert(QStringLiteral("distance"), pose.distanceMeters);
  tag.insert(QStringLiteral("pitch"), pose.euler.pitchX);
  tag.insert(QStringLiteral("yaw"), pose.euler.yawY);
  tag.insert(QStringLiteral("roll"), pose.euler.rollZ);
  return tag;
}

Matrix3 transpose(const Matrix3& matrix) {
  return {matrix[0], matrix[3], matrix[6], matrix[1], matrix[4],
          matrix[7], matrix[2], matrix[5], matrix[8]};
}

Matrix3 multiply(const Matrix3& left, const Matrix3& right) {
  Matrix3 result{};
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      result[row * 3 + column] = left[row * 3] * right[column] +
                                 left[row * 3 + 1] * right[3 + column] +
                                 left[row * 3 + 2] * right[6 + column];
    }
  }
  return result;
}

Vector3 multiply(const Matrix3& matrix, const Vector3& vector) {
  return {
      matrix[0] * vector[0] + matrix[1] * vector[1] + matrix[2] * vector[2],
      matrix[3] * vector[0] + matrix[4] * vector[1] + matrix[5] * vector[2],
      matrix[6] * vector[0] + matrix[7] * vector[1] + matrix[8] * vector[2]};
}

EulerAnglesDegrees eulerFromRotationMatrix(const Matrix3& rotationMatrix) {
  const double sy = std::sqrt((rotationMatrix[0] * rotationMatrix[0]) +
                              (rotationMatrix[6] * rotationMatrix[6]));
  const bool singular = sy < kSingularEulerThreshold;

  double pitch = 0.0;
  double yaw = 0.0;
  double roll = 0.0;

  if (!singular) {
    pitch = std::atan2(rotationMatrix[7], rotationMatrix[8]);
    yaw = std::atan2(-rotationMatrix[6], sy);
    roll = std::atan2(rotationMatrix[3], rotationMatrix[0]);
  } else {
    pitch = std::atan2(-rotationMatrix[5], rotationMatrix[4]);
    yaw = std::atan2(-rotationMatrix[6], sy);
    roll = 0.0;
  }

  return {pitch * kRadiansToDegrees, yaw * kRadiansToDegrees,
          roll * kRadiansToDegrees};
}

Vector3 positionVector(const AprilTagPose& pose) {
  return {pose.position.x, pose.position.y, pose.position.z};
}

double norm(const Vector3& vector) {
  return std::sqrt((vector[0] * vector[0]) + (vector[1] * vector[1]) +
                   (vector[2] * vector[2]));
}

std::optional<AprilTagPose> findPoseById(const std::vector<AprilTagPose>& poses,
                                         int tagId) {
  for (const AprilTagPose& pose : poses) {
    if (pose.id == tagId) {
      return pose;
    }
  }
  return std::nullopt;
}

AprilTagPose poseRelativeToHome(const AprilTagPose& pose,
                                const AprilTagPose& homePose) {
  const Matrix3 cameraToHomeRotation =
      transpose(homePose.rotationMatrixRowMajor);
  const Matrix3 openCvHomeToOpenCvTagRotation =
      multiply(cameraToHomeRotation, pose.rotationMatrixRowMajor);
  const Matrix3 homeToTagRotation = multiply(
      multiply(kOpenCvTagToFlatHomeBasis, openCvHomeToOpenCvTagRotation),
      kOpenCvTagToFlatHomeBasis);
  const Vector3 posePosition = positionVector(pose);
  const Vector3 homePosition = positionVector(homePose);
  const Vector3 cameraFrameDelta = {posePosition[0] - homePosition[0],
                                    posePosition[1] - homePosition[1],
                                    posePosition[2] - homePosition[2]};
  const Vector3 openCvHomeFramePosition =
      multiply(cameraToHomeRotation, cameraFrameDelta);
  const Vector3 homeFramePosition =
      multiply(kOpenCvTagToFlatHomeBasis, openCvHomeFramePosition);

  AprilTagPose relativePose;
  relativePose.id = pose.id;
  relativePose.position = {homeFramePosition[0], homeFramePosition[1],
                           homeFramePosition[2]};
  relativePose.rotationMatrixRowMajor = homeToTagRotation;
  relativePose.euler = eulerFromRotationMatrix(homeToTagRotation);
  relativePose.distanceMeters = norm(homeFramePosition);
  return relativePose;
}

std::vector<AprilTagPose> posesRelativeToHome(
    const std::vector<AprilTagPose>& poses, const AprilTagPose& homePose) {
  std::vector<AprilTagPose> relativePoses;
  relativePoses.reserve(poses.size());
  for (const AprilTagPose& pose : poses) {
    relativePoses.push_back(poseRelativeToHome(pose, homePose));
  }
  return relativePoses;
}

std::optional<AprilTagPose> findPoseById(const QVariantList&, int) = delete;

}  // namespace

TagVisualizationController::TagVisualizationController(
    jetsonqt::objectdetection::ObjectDetectionConfig config, int homeTagId,
    QObject* parent)
    : QObject(parent), detector_(std::move(config)), homeTagId_(homeTagId) {
  pollTimer_.setInterval(kPollingIntervalMs);
  connect(&pollTimer_, &QTimer::timeout, this,
          &TagVisualizationController::pollCamera);
}

TagVisualizationController::~TagVisualizationController() { stop(); }

QVariantList TagVisualizationController::tagPoses() const { return tagPoses_; }

QString TagVisualizationController::cameraImageSource() const {
  return cameraImageSource_;
}

QString TagVisualizationController::statusText() const { return statusText_; }

bool TagVisualizationController::cameraReady() const { return cameraReady_; }

bool TagVisualizationController::homeFrameAvailable() const {
  return homeFrameAvailable_;
}

int TagVisualizationController::homeTagId() const { return homeTagId_; }

bool TagVisualizationController::calibrated() const { return calibrated_; }

double TagVisualizationController::calibrationReprojectionError() const {
  return calibrationReprojectionError_;
}

QString TagVisualizationController::calibrationStatusText() const {
  return calibrationStatusText_;
}

QImage TagVisualizationController::latestCameraImage() const {
  return latestCameraImage_;
}

void TagVisualizationController::start() {
  if (pollTimer_.isActive()) {
    return;
  }

  std::string errorMessage;
  cameraReady_ = detector_.initializeCamera(&errorMessage);
  if (!cameraReady_) {
    homeFrameAvailable_ = false;
    statusText_ = QString::fromStdString(errorMessage);
    emit visualizationChanged();
    return;
  }

  calibrated_ = false;
  calibrationStatusText_ =
      QStringLiteral("Show the printed checkerboard to the camera.");
  statusText_ = QStringLiteral("Camera connected. Waiting for checkerboard.");
  emit visualizationChanged();

  pollCheckerboardCalibration();
  pollTimer_.start();
}

void TagVisualizationController::stop() {
  pollTimer_.stop();
  detector_.releaseCamera();
  cameraReady_ = false;
  homeFrameAvailable_ = false;
  calibrated_ = false;
  poseHistoryById_.clear();
}

void TagVisualizationController::pollCamera() {
  if (!calibrated_) {
    pollCheckerboardCalibration();
    return;
  }

  std::string errorMessage;
  const jetsonqt::objectdetection::AprilTagDetectionFrame frame =
      detector_.detectAprilTagsWithFrame(&errorMessage);
  if (!errorMessage.empty()) {
    homeFrameAvailable_ = false;
    statusText_ = QString::fromStdString(errorMessage);
    emit visualizationChanged();
    return;
  }

  const std::optional<AprilTagPose> homePose =
      findPoseById(frame.aprilTags, homeTagId_);
  if (homePose.has_value()) {
    const std::vector<AprilTagPose> relativePoses =
        posesRelativeToHome(frame.aprilTags, *homePose);
    tagPoses_ = toTagPoseList(averagePoses(relativePoses));
    homeFrameAvailable_ = true;
    statusText_ = QStringLiteral("%1 tag(s) visible. Home tag %2 frame active.")
                      .arg(frame.aprilTags.size())
                      .arg(homeTagId_);
  } else {
    tagPoses_ = {};
    poseHistoryById_.clear();
    homeFrameAvailable_ = false;
    statusText_ = QStringLiteral("%1 tag(s) visible. Home tag %2 not visible.")
                      .arg(frame.aprilTags.size())
                      .arg(homeTagId_);
  }

  if (frame.width > 0 && frame.height > 0 && !frame.rgbPixels.empty()) {
    updateCameraImage(frame.width, frame.height, frame.rgbPixels);
  }

  emit visualizationChanged();
}

void TagVisualizationController::pollCheckerboardCalibration() {
  std::string errorMessage;
  const jetsonqt::objectdetection::CheckerboardCalibrationFrame frame =
      detector_.calibrateFromCheckerboardFrame({}, &errorMessage);
  if (!errorMessage.empty()) {
    calibrationStatusText_ = QString::fromStdString(errorMessage);
    statusText_ = calibrationStatusText_;
    emit visualizationChanged();
    return;
  }

  updateCameraImage(frame.width, frame.height, frame.rgbPixels);

  if (!frame.checkerboardFound) {
    calibrationStatusText_ =
        QStringLiteral("Looking for an 8 x 6 inner-corner checkerboard.");
    statusText_ = calibrationStatusText_;
    emit visualizationChanged();
    return;
  }

  if (frame.calibrated) {
    calibrated_ = true;
    calibrationReprojectionError_ = frame.reprojectionError;
    calibrationStatusText_ =
        QStringLiteral("Calibration complete. Reprojection error %1 px.")
            .arg(calibrationReprojectionError_, 0, 'f', 3);
    statusText_ = calibrationStatusText_;
    emit visualizationChanged();
  }
}

void TagVisualizationController::updateCameraImage(
    int width, int height, const std::vector<unsigned char>& rgbPixels) {
  if (width <= 0 || height <= 0 || rgbPixels.empty()) {
    return;
  }

  const QImage image(rgbPixels.data(), width, height, width * kRgbChannelCount,
                     QImage::Format_RGB888);
  latestCameraImage_ = image.copy();
  frameRevision_ =
      frameRevision_ == 0 ? kFirstFrameRevision : frameRevision_ + 1;
  cameraImageSource_ =
      QStringLiteral("image://tagVisualizationCamera/live?rev=%1")
          .arg(frameRevision_);
  emit cameraImageChanged();
}

QVariantList TagVisualizationController::toTagPoseList(
    const std::vector<jetsonqt::objectdetection::AprilTagPose>& poses) const {
  QVariantList tags;
  tags.reserve(static_cast<int>(poses.size()));
  for (const jetsonqt::objectdetection::AprilTagPose& pose : poses) {
    tags.append(toTagPoseMap(pose));
  }
  return tags;
}

std::vector<AprilTagPose> TagVisualizationController::averagePoses(
    const std::vector<AprilTagPose>& poses) {
  std::set<int> visibleIds;
  for (const AprilTagPose& pose : poses) {
    visibleIds.insert(pose.id);
  }

  for (auto history = poseHistoryById_.begin();
       history != poseHistoryById_.end();) {
    if (visibleIds.count(history->first) == 0) {
      history = poseHistoryById_.erase(history);
      continue;
    }
    ++history;
  }

  std::vector<AprilTagPose> averaged;
  averaged.reserve(poses.size());

  for (const AprilTagPose& pose : poses) {
    std::deque<AprilTagPose>& history = poseHistoryById_[pose.id];
    history.push_back(pose);
    while (history.size() > kPoseAverageFrameCount) {
      history.pop_front();
    }

    AprilTagPose average = pose;
    average.position = {};
    average.euler = {};
    for (const AprilTagPose& sample : history) {
      average.position.x += sample.position.x;
      average.position.y += sample.position.y;
      average.position.z += sample.position.z;
      average.euler.pitchX += sample.euler.pitchX;
      average.euler.yawY += sample.euler.yawY;
      average.euler.rollZ += sample.euler.rollZ;
    }

    const double sampleCount = static_cast<double>(history.size());
    average.position.x /= sampleCount;
    average.position.y /= sampleCount;
    average.position.z /= sampleCount;
    average.euler.pitchX /= sampleCount;
    average.euler.yawY /= sampleCount;
    average.euler.rollZ /= sampleCount;
    average.distanceMeters =
        norm({average.position.x, average.position.y, average.position.z});
    averaged.push_back(average);
  }

  return averaged;
}

TagVisualizationImageProvider::TagVisualizationImageProvider(
    TagVisualizationController* controller)
    : QQuickImageProvider(QQuickImageProvider::Image),
      controller_(controller) {}

QImage TagVisualizationImageProvider::requestImage(const QString& id,
                                                   QSize* size,
                                                   const QSize& requestedSize) {
  Q_UNUSED(id)

  QImage image =
      controller_ == nullptr ? QImage() : controller_->latestCameraImage();
  if (size != nullptr) {
    *size = image.size();
  }

  if (requestedSize.isValid() && !image.isNull()) {
    return image.scaled(requestedSize, Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
  }

  return image;
}

}  // namespace tagvisualizationui

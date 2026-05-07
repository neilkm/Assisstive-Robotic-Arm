#include "TagVisualizationUi/TagVisualizationController.h"

#include <QSize>
#include <QVariantMap>

namespace tagvisualizationui {
namespace {

constexpr int kPollingIntervalMs = 150;
constexpr int kRgbChannelCount = 3;
constexpr int kFirstFrameRevision = 1;

QVariantMap toTagPoseMap(const jetsonqt::objectdetection::AprilTagPose& pose) {
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

}  // namespace

TagVisualizationController::TagVisualizationController(
    jetsonqt::objectdetection::ObjectDetectionConfig config, QObject* parent)
    : QObject(parent), detector_(std::move(config)) {
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
    statusText_ = QString::fromStdString(errorMessage);
    emit visualizationChanged();
    return;
  }

  statusText_ = QStringLiteral("Camera connected.");
  emit visualizationChanged();

  pollCamera();
  pollTimer_.start();
}

void TagVisualizationController::stop() {
  pollTimer_.stop();
  detector_.releaseCamera();
  cameraReady_ = false;
}

void TagVisualizationController::pollCamera() {
  std::string errorMessage;
  const jetsonqt::objectdetection::AprilTagDetectionFrame frame =
      detector_.detectAprilTagsWithFrame(&errorMessage);
  if (!errorMessage.empty()) {
    statusText_ = QString::fromStdString(errorMessage);
    emit visualizationChanged();
    return;
  }

  tagPoses_ = toTagPoseList(frame.aprilTags);
  statusText_ = QStringLiteral("%1 tag(s) visible.").arg(tagPoses_.size());

  if (frame.width > 0 && frame.height > 0 && !frame.rgbPixels.empty()) {
    const QImage image(frame.rgbPixels.data(), frame.width, frame.height,
                       frame.width * kRgbChannelCount, QImage::Format_RGB888);
    latestCameraImage_ = image.copy();
    frameRevision_ =
        frameRevision_ == 0 ? kFirstFrameRevision : frameRevision_ + 1;
    cameraImageSource_ =
        QStringLiteral("image://tagVisualizationCamera/live?rev=%1")
            .arg(frameRevision_);
    emit cameraImageChanged();
  }

  emit visualizationChanged();
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

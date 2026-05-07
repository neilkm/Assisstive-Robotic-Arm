#pragma once

#include <QImage>
#include <QObject>
#include <QQuickImageProvider>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "JetsonQtApp/ObjectDetection/ObjectDetection.h"

namespace tagvisualizationui {

class TagVisualizationController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList tagPoses READ tagPoses NOTIFY visualizationChanged)
  Q_PROPERTY(QString cameraImageSource READ cameraImageSource NOTIFY
                 cameraImageChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY visualizationChanged)
  Q_PROPERTY(bool cameraReady READ cameraReady NOTIFY visualizationChanged)
  Q_PROPERTY(bool homeFrameAvailable READ homeFrameAvailable NOTIFY
                 visualizationChanged)
  Q_PROPERTY(int homeTagId READ homeTagId CONSTANT)
  Q_PROPERTY(bool calibrated READ calibrated NOTIFY visualizationChanged)
  Q_PROPERTY(double calibrationReprojectionError READ
                 calibrationReprojectionError NOTIFY visualizationChanged)
  Q_PROPERTY(QString calibrationStatusText READ calibrationStatusText NOTIFY
                 visualizationChanged)

 public:
  explicit TagVisualizationController(
      jetsonqt::objectdetection::ObjectDetectionConfig config,
      int homeTagId = 5, QObject* parent = nullptr);
  ~TagVisualizationController() override;

  [[nodiscard]] QVariantList tagPoses() const;
  [[nodiscard]] QString cameraImageSource() const;
  [[nodiscard]] QString statusText() const;
  [[nodiscard]] bool cameraReady() const;
  [[nodiscard]] bool homeFrameAvailable() const;
  [[nodiscard]] int homeTagId() const;
  [[nodiscard]] bool calibrated() const;
  [[nodiscard]] double calibrationReprojectionError() const;
  [[nodiscard]] QString calibrationStatusText() const;
  [[nodiscard]] QImage latestCameraImage() const;

  Q_INVOKABLE void start();
  Q_INVOKABLE void stop();

 signals:
  void visualizationChanged();
  void cameraImageChanged();

 private:
  void pollCamera();
  void pollCheckerboardCalibration();
  void updateCameraImage(int width, int height,
                         const std::vector<unsigned char>& rgbPixels);
  [[nodiscard]] QVariantList toTagPoseList(
      const std::vector<jetsonqt::objectdetection::AprilTagPose>& poses) const;
  [[nodiscard]] std::vector<jetsonqt::objectdetection::AprilTagPose>
  smoothPoses(
      const std::vector<jetsonqt::objectdetection::AprilTagPose>& poses);

  jetsonqt::objectdetection::ObjectDetection detector_;
  QTimer pollTimer_;
  QVariantList tagPoses_;
  QImage latestCameraImage_;
  QString cameraImageSource_;
  QString statusText_;
  QString calibrationStatusText_;
  std::vector<jetsonqt::objectdetection::AprilTagPose> smoothedPoses_;
  double calibrationReprojectionError_ = 0.0;
  int frameRevision_ = 0;
  int homeTagId_ = 5;
  bool cameraReady_ = false;
  bool homeFrameAvailable_ = false;
  bool calibrated_ = false;
};

class TagVisualizationImageProvider final : public QQuickImageProvider {
 public:
  explicit TagVisualizationImageProvider(
      TagVisualizationController* controller);

  [[nodiscard]] QImage requestImage(const QString& id, QSize* size,
                                    const QSize& requestedSize) override;

 private:
  TagVisualizationController* controller_ = nullptr;
};

}  // namespace tagvisualizationui

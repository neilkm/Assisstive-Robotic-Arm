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

 public:
  explicit TagVisualizationController(
      jetsonqt::objectdetection::ObjectDetectionConfig config,
      QObject* parent = nullptr);
  ~TagVisualizationController() override;

  [[nodiscard]] QVariantList tagPoses() const;
  [[nodiscard]] QString cameraImageSource() const;
  [[nodiscard]] QString statusText() const;
  [[nodiscard]] bool cameraReady() const;
  [[nodiscard]] QImage latestCameraImage() const;

  Q_INVOKABLE void start();
  Q_INVOKABLE void stop();

 signals:
  void visualizationChanged();
  void cameraImageChanged();

 private:
  void pollCamera();
  [[nodiscard]] QVariantList toTagPoseList(
      const std::vector<jetsonqt::objectdetection::AprilTagPose>& poses) const;

  jetsonqt::objectdetection::ObjectDetection detector_;
  QTimer pollTimer_;
  QVariantList tagPoses_;
  QImage latestCameraImage_;
  QString cameraImageSource_;
  QString statusText_;
  int frameRevision_ = 0;
  bool cameraReady_ = false;
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

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace jetsonqt::objectdetection {

inline constexpr int kDefaultCameraIndex = 0;
inline constexpr int kDefaultCaptureBackend = 0;
inline constexpr double kDefaultAprilTagSizeMeters = 0.10;
inline constexpr bool kDefaultUseApproximateIntrinsicsWhenUncalibrated = true;
inline constexpr int kInvalidAprilTagId = -1;

struct ObjectDetectionConfig {
  int cameraIndex = kDefaultCameraIndex;
  int captureBackend = kDefaultCaptureBackend;
  double aprilTagSizeMeters = kDefaultAprilTagSizeMeters;
  std::string calibrationFilePath;
  bool useApproximateIntrinsicsWhenUncalibrated =
      kDefaultUseApproximateIntrinsicsWhenUncalibrated;
};

struct EulerAnglesDegrees {
  double pitchX = 0.0;
  double yawY = 0.0;
  double rollZ = 0.0;
};

struct PositionMeters {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct AprilTagPose {
  int id = kInvalidAprilTagId;
  PositionMeters position;
  EulerAnglesDegrees euler;
  double distanceMeters = 0.0;
};

class ObjectDetection {
 public:
  explicit ObjectDetection(ObjectDetectionConfig config = {});
  ~ObjectDetection();

  ObjectDetection(const ObjectDetection&) = delete;
  ObjectDetection& operator=(const ObjectDetection&) = delete;
  ObjectDetection(ObjectDetection&&) noexcept;
  ObjectDetection& operator=(ObjectDetection&&) noexcept;

  [[nodiscard]] bool initializeCamera(std::string* errorMessage = nullptr);
  [[nodiscard]] bool isCameraInitialized() const;
  void releaseCamera();

  // Captures one frame, detects every visible AprilTag 36h11 marker, and solves
  // each tag pose in the OpenCV camera frame: +x right, +y down, +z forward.
  [[nodiscard]] std::vector<AprilTagPose> detectAprilTags(
      std::string* errorMessage = nullptr);

  [[nodiscard]] const ObjectDetectionConfig& config() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace jetsonqt::objectdetection

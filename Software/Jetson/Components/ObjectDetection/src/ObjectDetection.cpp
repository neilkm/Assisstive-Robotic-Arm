#include "JetsonQtApp/ObjectDetection/ObjectDetection.h"

#include <cmath>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/imgproc.hpp>
#if defined(OBJECT_DETECTION_USE_OPENCV_OBJDETECT_ARUCO)
#include <opencv2/objdetect/aruco_detector.hpp>
#else
#include <opencv2/aruco.hpp>
#endif
#include <opencv2/videoio.hpp>
#include <sstream>
#include <utility>

namespace jetsonqt::objectdetection {
namespace {

constexpr double kSingularEulerThreshold = 1e-6;
constexpr int kCameraMatrixRows = 3;
constexpr int kCameraMatrixCols = 3;
constexpr int kDefaultDistortionCoefficientCount = 5;
constexpr double kApproximateFocalLengthScale = 0.9;
constexpr double kHalfScale = 0.5;
constexpr double kRadiansToDegrees = 180.0 / CV_PI;
constexpr double kZeroDepthMeters = 0.0;
constexpr int kNoCaptureBackend = 0;
constexpr int kFirstMatrixRow = 0;
constexpr int kSecondMatrixRow = 1;
constexpr int kThirdMatrixRow = 2;
constexpr int kFirstMatrixCol = 0;
constexpr int kSecondMatrixCol = 1;
constexpr int kThirdMatrixCol = 2;
constexpr int kFirstDistortionRow = 0;
constexpr int kFirstVectorElement = 0;
constexpr int kSecondVectorElement = 1;
constexpr int kThirdVectorElement = 2;
constexpr int kRgbChannelCount = 3;

void setError(std::string* errorMessage, const std::string& message) {
  if (errorMessage != nullptr) {
    *errorMessage = message;
  }
}

bool readCalibrationFile(const std::string& path, cv::Mat* cameraMatrix,
                         cv::Mat* distortionCoefficients,
                         std::string* errorMessage) {
  cv::FileStorage storage(path, cv::FileStorage::READ);
  if (!storage.isOpened()) {
    setError(errorMessage, "Could not open camera calibration file: " + path);
    return false;
  }

  storage["camera_matrix"] >> *cameraMatrix;
  storage["dist_coeffs"] >> *distortionCoefficients;

  if (cameraMatrix->empty() || cameraMatrix->rows != kCameraMatrixRows ||
      cameraMatrix->cols != kCameraMatrixCols) {
    setError(errorMessage,
             "Calibration file must contain a 3x3 camera_matrix in OpenCV "
             "FileStorage format.");
    return false;
  }

  if (distortionCoefficients->empty()) {
    *distortionCoefficients = cv::Mat::zeros(kDefaultDistortionCoefficientCount,
                                             kFirstDistortionRow + 1, CV_64F);
  }

  cameraMatrix->convertTo(*cameraMatrix, CV_64F);
  distortionCoefficients->convertTo(*distortionCoefficients, CV_64F);
  return true;
}

EulerAnglesDegrees eulerFromRotationMatrix(const cv::Mat& rotationMatrix) {
  const double r00 =
      rotationMatrix.at<double>(kFirstMatrixRow, kFirstMatrixCol);
  const double r10 =
      rotationMatrix.at<double>(kSecondMatrixRow, kFirstMatrixCol);
  const double r20 =
      rotationMatrix.at<double>(kThirdMatrixRow, kFirstMatrixCol);
  const double r21 =
      rotationMatrix.at<double>(kThirdMatrixRow, kSecondMatrixCol);
  const double r22 =
      rotationMatrix.at<double>(kThirdMatrixRow, kThirdMatrixCol);
  const double r12 =
      rotationMatrix.at<double>(kSecondMatrixRow, kThirdMatrixCol);
  const double r11 =
      rotationMatrix.at<double>(kSecondMatrixRow, kSecondMatrixCol);

  const double sy = std::sqrt((r00 * r00) + (r20 * r20));
  const bool singular = sy < kSingularEulerThreshold;

  double pitch = 0.0;
  double yaw = 0.0;
  double roll = 0.0;

  if (!singular) {
    pitch = std::atan2(r21, r22);
    yaw = std::atan2(-r20, sy);
    roll = std::atan2(r10, r00);
  } else {
    pitch = std::atan2(-r12, r11);
    yaw = std::atan2(-r20, sy);
    roll = 0.0;
  }

  return {
      pitch * kRadiansToDegrees,
      yaw * kRadiansToDegrees,
      roll * kRadiansToDegrees,
  };
}

std::vector<cv::Point3d> tagObjectPoints(double tagSizeMeters) {
  const double half = tagSizeMeters * kHalfScale;
  return {
      {-half, -half, kZeroDepthMeters},
      {half, -half, kZeroDepthMeters},
      {half, half, kZeroDepthMeters},
      {-half, half, kZeroDepthMeters},
  };
}

void detectAprilTagCorners(const cv::Mat& grayFrame,
                           std::vector<std::vector<cv::Point2f>>* corners,
                           std::vector<int>* ids) {
#if defined(OBJECT_DETECTION_USE_OPENCV_OBJDETECT_ARUCO)
  const cv::aruco::Dictionary dictionary =
      cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);
  const cv::aruco::DetectorParameters detectorParameters;
  cv::aruco::ArucoDetector detector(dictionary, detectorParameters);
  detector.detectMarkers(grayFrame, *corners, *ids);
#else
  const cv::Ptr<cv::aruco::Dictionary> dictionary =
      cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);
  const cv::Ptr<cv::aruco::DetectorParameters> detectorParameters =
      cv::aruco::DetectorParameters::create();
  cv::aruco::detectMarkers(grayFrame, dictionary, *corners, *ids,
                           detectorParameters);
#endif
}

std::vector<AprilTagPose> solveTagPoses(
    const std::vector<std::vector<cv::Point2f>>& corners,
    const std::vector<int>& ids, double aprilTagSizeMeters,
    const cv::Mat& cameraMatrix, const cv::Mat& distortionCoefficients) {
  std::vector<AprilTagPose> poses;
  poses.reserve(ids.size());

  // The object points model the four tag corners around the tag center.
  // solvePnP returns each tag center's rotation and translation relative to the
  // camera.
  const std::vector<cv::Point3d> objectPoints =
      tagObjectPoints(aprilTagSizeMeters);
  for (std::size_t i = 0; i < ids.size(); ++i) {
    cv::Vec3d rotationVector;
    cv::Vec3d translationVector;

    const bool solved = cv::solvePnP(
        objectPoints, corners[i], cameraMatrix, distortionCoefficients,
        rotationVector, translationVector, false, cv::SOLVEPNP_ITERATIVE);
    if (!solved) {
      continue;
    }

    cv::Mat rotationMatrix;
    cv::Rodrigues(rotationVector, rotationMatrix);

    AprilTagPose pose;
    pose.id = ids[i];
    pose.position = {translationVector[kFirstVectorElement],
                     translationVector[kSecondVectorElement],
                     translationVector[kThirdVectorElement]};
    pose.euler = eulerFromRotationMatrix(rotationMatrix);
    pose.rotationMatrixRowMajor = {
        rotationMatrix.at<double>(kFirstMatrixRow, kFirstMatrixCol),
        rotationMatrix.at<double>(kFirstMatrixRow, kSecondMatrixCol),
        rotationMatrix.at<double>(kFirstMatrixRow, kThirdMatrixCol),
        rotationMatrix.at<double>(kSecondMatrixRow, kFirstMatrixCol),
        rotationMatrix.at<double>(kSecondMatrixRow, kSecondMatrixCol),
        rotationMatrix.at<double>(kSecondMatrixRow, kThirdMatrixCol),
        rotationMatrix.at<double>(kThirdMatrixRow, kFirstMatrixCol),
        rotationMatrix.at<double>(kThirdMatrixRow, kSecondMatrixCol),
        rotationMatrix.at<double>(kThirdMatrixRow, kThirdMatrixCol)};
    pose.distanceMeters = cv::norm(cv::Mat(translationVector), cv::NORM_L2);

    poses.push_back(pose);
  }

  return poses;
}

}  // namespace

struct ObjectDetection::Impl {
  explicit Impl(ObjectDetectionConfig objectDetectionConfig)
      : config(std::move(objectDetectionConfig)) {}

  bool initializeIntrinsicsForFrame(const cv::Mat& frame,
                                    std::string* errorMessage) {
    if (intrinsicsInitialized) {
      return true;
    }

    if (!config.calibrationFilePath.empty()) {
      // A calibrated camera matrix is required for meaningful metric pose
      // estimates.
      if (!readCalibrationFile(config.calibrationFilePath, &cameraMatrix,
                               &distortionCoefficients, errorMessage)) {
        return false;
      }
      intrinsicsInitialized = true;
      return true;
    }

    if (!config.useApproximateIntrinsicsWhenUncalibrated) {
      setError(errorMessage, "No camera calibration was provided.");
      return false;
    }

    // This fallback keeps the camera smoke test usable before calibration. The
    // resulting translation values are approximate and should not be used for
    // final robot geometry.
    const double width = static_cast<double>(frame.cols);
    const double height = static_cast<double>(frame.rows);
    const double focalLength = kApproximateFocalLengthScale * width;

    cameraMatrix =
        (cv::Mat_<double>(kCameraMatrixRows, kCameraMatrixCols) << focalLength,
         0.0, width * kHalfScale, 0.0, focalLength, height * kHalfScale, 0.0,
         0.0, 1.0);
    distortionCoefficients = cv::Mat::zeros(kDefaultDistortionCoefficientCount,
                                            kFirstDistortionRow + 1, CV_64F);
    intrinsicsInitialized = true;
    return true;
  }

  ObjectDetectionConfig config;
  cv::VideoCapture capture;
  cv::Mat cameraMatrix;
  cv::Mat distortionCoefficients;
  bool intrinsicsInitialized = false;
};

ObjectDetection::ObjectDetection(ObjectDetectionConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

ObjectDetection::~ObjectDetection() = default;

ObjectDetection::ObjectDetection(ObjectDetection&&) noexcept = default;

ObjectDetection& ObjectDetection::operator=(ObjectDetection&&) noexcept =
    default;

bool ObjectDetection::initializeCamera(std::string* errorMessage) {
  if (impl_->config.aprilTagSizeMeters <= 0.0) {
    setError(errorMessage, "AprilTag size must be greater than zero meters.");
    return false;
  }

  // captureBackend stays optional so callers can use OpenCV defaults on
  // development hosts and request a specific backend, such as V4L2, on Jetson
  // when needed.
  bool opened = false;
  if (impl_->config.captureBackend == kNoCaptureBackend) {
    opened = impl_->capture.open(impl_->config.cameraIndex);
  } else {
    opened = impl_->capture.open(impl_->config.cameraIndex,
                                 impl_->config.captureBackend);
  }

  if (!opened || !impl_->capture.isOpened()) {
    std::ostringstream message;
    message << "Could not open camera index " << impl_->config.cameraIndex
            << ".";
    setError(errorMessage, message.str());
    return false;
  }

  // Read one frame during initialization so camera failures surface before the
  // polling loop.
  cv::Mat frame;
  if (!impl_->capture.read(frame) || frame.empty()) {
    setError(errorMessage, "Camera opened but did not return a frame.");
    impl_->capture.release();
    return false;
  }

  if (!impl_->initializeIntrinsicsForFrame(frame, errorMessage)) {
    impl_->capture.release();
    return false;
  }

  return true;
}

bool ObjectDetection::isCameraInitialized() const {
  return impl_->capture.isOpened();
}

void ObjectDetection::releaseCamera() { impl_->capture.release(); }

std::vector<AprilTagPose> ObjectDetection::detectAprilTags(
    std::string* errorMessage) {
  return detectAprilTagsWithFrame(errorMessage).aprilTags;
}

AprilTagDetectionFrame ObjectDetection::detectAprilTagsWithFrame(
    std::string* errorMessage) {
  AprilTagDetectionFrame detectionFrame;

  if (!impl_->capture.isOpened()) {
    setError(errorMessage, "Camera is not initialized.");
    return detectionFrame;
  }

  cv::Mat frame;
  if (!impl_->capture.read(frame) || frame.empty()) {
    setError(errorMessage, "Could not read a frame from the camera.");
    return detectionFrame;
  }

  if (!impl_->initializeIntrinsicsForFrame(frame, errorMessage)) {
    return detectionFrame;
  }

  cv::Mat grayFrame;
  cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);

  std::vector<std::vector<cv::Point2f>> corners;
  std::vector<int> ids;
  detectAprilTagCorners(grayFrame, &corners, &ids);

  cv::Mat rgbFrame;
  cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
  if (!rgbFrame.isContinuous()) {
    rgbFrame = rgbFrame.clone();
  }

  detectionFrame.width = rgbFrame.cols;
  detectionFrame.height = rgbFrame.rows;
  detectionFrame.rgbPixels.assign(
      rgbFrame.datastart,
      rgbFrame.datastart +
          (static_cast<std::size_t>(rgbFrame.cols) *
           static_cast<std::size_t>(rgbFrame.rows) * kRgbChannelCount));
  detectionFrame.aprilTags =
      solveTagPoses(corners, ids, impl_->config.aprilTagSizeMeters,
                    impl_->cameraMatrix, impl_->distortionCoefficients);

  if (errorMessage != nullptr) {
    errorMessage->clear();
  }
  return detectionFrame;
}

const ObjectDetectionConfig& ObjectDetection::config() const {
  return impl_->config;
}

}  // namespace jetsonqt::objectdetection

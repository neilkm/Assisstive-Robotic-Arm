#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include "JetsonQtApp/ObjectDetection/ObjectDetection.h"

namespace {

using jetsonqt::objectdetection::EulerAnglesDegrees;
using jetsonqt::objectdetection::ObjectDetection;
using jetsonqt::objectdetection::ObjectDetectionConfig;

using Matrix3 = std::array<double, 9>;
using Vector3 = std::array<double, 3>;

// OpenCV's tag-local Y/Z directions are converted so the home tag lies in the
// XY plane and +Z points up out of the printed tag surface.
constexpr Matrix3 kOpenCvTagToFlatHomeBasis = {1.0, 0.0, 0.0, 0.0, -1.0,
                                               0.0, 0.0, 0.0, -1.0};

constexpr int kRunUntilInterrupted = 0;
constexpr int kFirstPollingCycle = 1;
constexpr int kMinimumFiniteCycleCount = 0;
constexpr int kSuccessfulExit = 0;
constexpr int kFailedExit = 1;
constexpr int kOutputPrecision = 4;
constexpr int kDefaultHomeTagId = 5;
constexpr double kSingularEulerThreshold = 1e-6;
constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;
constexpr const char* kDefaultCalibrationRelativePath =
    "Software/Jetson/configs/logitech_c270_camera.yaml";
constexpr auto kPollingInterval = std::chrono::seconds(1);

struct TestOptions {
  ObjectDetectionConfig config;
  int cycles = kRunUntilInterrupted;
  std::optional<int> homeTagId = kDefaultHomeTagId;
};

int parseInt(const std::string& value, const std::string& argumentName) {
  try {
    std::size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    if (consumed != value.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return parsed;
  } catch (const std::exception&) {
    throw std::runtime_error("Invalid integer for " + argumentName + ": " +
                             value);
  }
}

double parseDouble(const std::string& value, const std::string& argumentName) {
  try {
    std::size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    if (consumed != value.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return parsed;
  } catch (const std::exception&) {
    throw std::runtime_error("Invalid number for " + argumentName + ": " +
                             value);
  }
}

std::optional<std::filesystem::path> findRepoFile(
    const std::filesystem::path& relativePath) {
  std::filesystem::path directory = std::filesystem::current_path();
  while (true) {
    const std::filesystem::path candidate = directory / relativePath;
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }

    if (!directory.has_parent_path() || directory.parent_path() == directory) {
      return std::nullopt;
    }
    directory = directory.parent_path();
  }
}

std::string defaultCalibrationPath() {
  const std::optional<std::filesystem::path> calibrationPath =
      findRepoFile(kDefaultCalibrationRelativePath);
  return calibrationPath.has_value() ? calibrationPath->string()
                                     : std::string{};
}

void printUsage(const char* executableName) {
  std::cout << "Usage: " << executableName << " [options]\n"
            << "\n"
            << "Options:\n"
            << "  --camera-index N       OpenCV camera index. Default: 0\n"
            << "  --tag-size-m METERS    AprilTag black-square edge length. "
               "Default: 0.0254\n"
            << "  --calibration PATH     OpenCV YAML with camera_matrix and "
               "dist_coeffs.\n"
            << "  --home-tag-id N        Express all tag poses relative to "
               "this visible AprilTag. Default: 5\n"
            << "  --cycles N             Stop after N one-second polling "
               "cycles. Default: run until Ctrl+C\n"
            << "  --help                 Show this help text.\n";
}

TestOptions parseOptions(int argc, char** argv) {
  TestOptions options;
  options.config.calibrationFilePath = defaultCalibrationPath();

  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];

    if (argument == "--help") {
      printUsage(argv[0]);
      std::exit(0);
    }

    if (i + 1 >= argc) {
      throw std::runtime_error("Missing value for " + argument);
    }

    const std::string value = argv[++i];
    if (argument == "--camera-index") {
      options.config.cameraIndex = parseInt(value, argument);
    } else if (argument == "--tag-size-m") {
      options.config.aprilTagSizeMeters = parseDouble(value, argument);
    } else if (argument == "--calibration") {
      options.config.calibrationFilePath = value;
    } else if (argument == "--home-tag-id") {
      options.homeTagId = parseInt(value, argument);
      if (*options.homeTagId < 0) {
        throw std::runtime_error("--home-tag-id must be non-negative.");
      }
    } else if (argument == "--cycles") {
      options.cycles = parseInt(value, argument);
      if (options.cycles < kMinimumFiniteCycleCount) {
        throw std::runtime_error("--cycles must be zero or greater.");
      }
    } else {
      throw std::runtime_error("Unknown argument: " + argument);
    }
  }

  return options;
}

Matrix3 transpose(const Matrix3& matrix) {
  return {matrix[0], matrix[3], matrix[6], matrix[1], matrix[4],
          matrix[7], matrix[2], matrix[5], matrix[8]};
}

Matrix3 multiply(const Matrix3& left, const Matrix3& right) {
  Matrix3 result{};
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      result[row * 3 + col] = left[row * 3] * right[col] +
                              left[row * 3 + 1] * right[3 + col] +
                              left[row * 3 + 2] * right[6 + col];
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

Vector3 positionVector(const jetsonqt::objectdetection::AprilTagPose& pose) {
  return {pose.position.x, pose.position.y, pose.position.z};
}

double norm(const Vector3& vector) {
  return std::sqrt((vector[0] * vector[0]) + (vector[1] * vector[1]) +
                   (vector[2] * vector[2]));
}

std::optional<jetsonqt::objectdetection::AprilTagPose> findPoseById(
    const std::vector<jetsonqt::objectdetection::AprilTagPose>& poses,
    int tagId) {
  for (const jetsonqt::objectdetection::AprilTagPose& pose : poses) {
    if (pose.id == tagId) {
      return pose;
    }
  }
  return std::nullopt;
}

jetsonqt::objectdetection::AprilTagPose poseRelativeToHome(
    const jetsonqt::objectdetection::AprilTagPose& pose,
    const jetsonqt::objectdetection::AprilTagPose& homePose) {
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

  jetsonqt::objectdetection::AprilTagPose relativePose;
  relativePose.id = pose.id;
  relativePose.position = {homeFramePosition[0], homeFramePosition[1],
                           homeFramePosition[2]};
  relativePose.rotationMatrixRowMajor = homeToTagRotation;
  relativePose.euler = eulerFromRotationMatrix(homeToTagRotation);
  relativePose.distanceMeters = norm(homeFramePosition);
  return relativePose;
}

std::vector<jetsonqt::objectdetection::AprilTagPose> posesRelativeToHome(
    const std::vector<jetsonqt::objectdetection::AprilTagPose>& poses,
    const jetsonqt::objectdetection::AprilTagPose& homePose) {
  std::vector<jetsonqt::objectdetection::AprilTagPose> relativePoses;
  relativePoses.reserve(poses.size());
  for (const jetsonqt::objectdetection::AprilTagPose& pose : poses) {
    relativePoses.push_back(poseRelativeToHome(pose, homePose));
  }
  return relativePoses;
}

void printPoseCycle(
    int cycle,
    const std::vector<jetsonqt::objectdetection::AprilTagPose>& poses,
    const std::string& frameName) {
  std::cout << "cycle=" << cycle << " visible_tags=" << poses.size() << '\n';
  std::cout << "  coordinate_frame=" << frameName << '\n';

  if (poses.empty()) {
    std::cout << "  no AprilTags detected\n";
    return;
  }

  std::cout << std::fixed << std::setprecision(kOutputPrecision);
  for (const auto& pose : poses) {
    std::cout << "  id=" << pose.id << " position_m=(" << pose.position.x
              << ", " << pose.position.y << ", " << pose.position.z << ")"
              << " distance_m=" << pose.distanceMeters
              << " euler_deg=(pitch_x=" << pose.euler.pitchX
              << ", yaw_y=" << pose.euler.yawY
              << ", roll_z=" << pose.euler.rollZ << ")\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const TestOptions options = parseOptions(argc, argv);
    const ObjectDetectionConfig& config = options.config;

    ObjectDetection detector(config);

    std::string errorMessage;
    if (!detector.initializeCamera(&errorMessage)) {
      std::cerr << "Object detection camera test failed: " << errorMessage
                << '\n';
      return kFailedExit;
    }

    std::cout << "Object detection camera test started.\n"
              << "camera_index=" << config.cameraIndex
              << " tag_size_m=" << config.aprilTagSizeMeters << " calibration="
              << (config.calibrationFilePath.empty()
                      ? "approximate"
                      : config.calibrationFilePath)
              << " home_tag_id="
              << (options.homeTagId.has_value()
                      ? std::to_string(*options.homeTagId)
                      : "none")
              << '\n';

    int cycle = kFirstPollingCycle;
    while (options.cycles == kRunUntilInterrupted || cycle <= options.cycles) {
      const auto cycleStart = std::chrono::steady_clock::now();

      // Poll once per second and print only terminal output for hardware
      // bring-up.
      std::vector<jetsonqt::objectdetection::AprilTagPose> poses =
          detector.detectAprilTags(&errorMessage);
      if (!errorMessage.empty()) {
        std::cerr << "cycle=" << cycle << " error=" << errorMessage << '\n';
      } else {
        if (options.homeTagId.has_value()) {
          const std::optional<jetsonqt::objectdetection::AprilTagPose>
              homePose = findPoseById(poses, *options.homeTagId);
          if (!homePose.has_value()) {
            std::cout << "cycle=" << cycle << " visible_tags=" << poses.size()
                      << '\n'
                      << "  home_tag_id=" << *options.homeTagId
                      << " not visible; relative frame unavailable\n";
          } else {
            printPoseCycle(cycle, posesRelativeToHome(poses, *homePose),
                           "home_tag_" + std::to_string(*options.homeTagId));
          }
        } else {
          printPoseCycle(cycle, poses, "camera");
        }
      }

      std::cout << std::flush;
      ++cycle;
      std::this_thread::sleep_until(cycleStart + kPollingInterval);
    }

    detector.releaseCamera();
    return kSuccessfulExit;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return kFailedExit;
  }
}

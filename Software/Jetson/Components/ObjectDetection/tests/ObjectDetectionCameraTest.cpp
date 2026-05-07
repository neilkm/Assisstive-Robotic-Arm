#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "JetsonQtApp/ObjectDetection/ObjectDetection.h"

namespace {

using jetsonqt::objectdetection::ObjectDetection;
using jetsonqt::objectdetection::ObjectDetectionConfig;

constexpr int kRunUntilInterrupted = 0;
constexpr int kFirstPollingCycle = 1;
constexpr int kMinimumFiniteCycleCount = 0;
constexpr int kSuccessfulExit = 0;
constexpr int kFailedExit = 1;
constexpr int kOutputPrecision = 4;
constexpr auto kPollingInterval = std::chrono::seconds(1);

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

void printUsage(const char* executableName) {
  std::cout << "Usage: " << executableName << " [options]\n"
            << "\n"
            << "Options:\n"
            << "  --camera-index N       OpenCV camera index. Default: 0\n"
            << "  --tag-size-m METERS    AprilTag black-square edge length. "
               "Default: 0.10\n"
            << "  --calibration PATH     OpenCV YAML with camera_matrix and "
               "dist_coeffs.\n"
            << "  --cycles N             Stop after N one-second polling "
               "cycles. Default: run until Ctrl+C\n"
            << "  --help                 Show this help text.\n";
}

ObjectDetectionConfig parseConfig(int argc, char** argv, int* cycles) {
  ObjectDetectionConfig config;
  *cycles = kRunUntilInterrupted;

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
      config.cameraIndex = parseInt(value, argument);
    } else if (argument == "--tag-size-m") {
      config.aprilTagSizeMeters = parseDouble(value, argument);
    } else if (argument == "--calibration") {
      config.calibrationFilePath = value;
    } else if (argument == "--cycles") {
      *cycles = parseInt(value, argument);
      if (*cycles < kMinimumFiniteCycleCount) {
        throw std::runtime_error("--cycles must be zero or greater.");
      }
    } else {
      throw std::runtime_error("Unknown argument: " + argument);
    }
  }

  return config;
}

void printPoseCycle(
    int cycle,
    const std::vector<jetsonqt::objectdetection::AprilTagPose>& poses) {
  std::cout << "cycle=" << cycle << " visible_tags=" << poses.size() << '\n';

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
    int cycles = 0;
    const ObjectDetectionConfig config = parseConfig(argc, argv, &cycles);

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
              << '\n';

    int cycle = kFirstPollingCycle;
    while (cycles == kRunUntilInterrupted || cycle <= cycles) {
      const auto cycleStart = std::chrono::steady_clock::now();

      // Poll once per second and print only terminal output for hardware
      // bring-up.
      std::vector<jetsonqt::objectdetection::AprilTagPose> poses =
          detector.detectAprilTags(&errorMessage);
      if (!errorMessage.empty()) {
        std::cerr << "cycle=" << cycle << " error=" << errorMessage << '\n';
      } else {
        printPoseCycle(cycle, poses);
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

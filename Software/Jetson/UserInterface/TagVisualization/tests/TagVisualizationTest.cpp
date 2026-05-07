#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#include "TagVisualizationUi/TagVisualizationController.h"

namespace {

constexpr int kSuccessfulExit = 0;
constexpr int kFailedExit = 1;
constexpr int kDefaultHomeTagId = 5;
constexpr const char* kDefaultCalibrationRelativePath =
    "Software/Jetson/configs/logitech_c270_camera.yaml";

struct TestOptions {
  jetsonqt::objectdetection::ObjectDetectionConfig config;
  int homeTagId = kDefaultHomeTagId;
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

TestOptions parseOptions(int argc, char** argv) {
  TestOptions options;
  options.config.calibrationFilePath = defaultCalibrationPath();

  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
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
      if (options.homeTagId < 0) {
        throw std::runtime_error("--home-tag-id must be non-negative.");
      }
    } else {
      throw std::runtime_error("Unknown argument: " + argument);
    }
  }

  return options;
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    QGuiApplication app(argc, argv);
    const TestOptions options = parseOptions(argc, argv);

    tagvisualizationui::TagVisualizationController controller(
        options.config, options.homeTagId);

    QQmlApplicationEngine engine;
    engine.addImageProvider(
        QStringLiteral("tagVisualizationCamera"),
        new tagvisualizationui::TagVisualizationImageProvider(&controller));
    engine.rootContext()->setContextProperty(QStringLiteral("tagVisualization"),
                                             &controller);
    engine.load(QUrl(QStringLiteral("qrc:/TagVisualization_Test/main.qml")));

    if (engine.rootObjects().isEmpty()) {
      return kFailedExit;
    }

    controller.start();
    return QGuiApplication::exec();
  } catch (const std::exception& error) {
    qCritical("%s", error.what());
    return kFailedExit;
  }
}

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "TagVisualizationUi/TagVisualizationController.h"

namespace {

constexpr int kSuccessfulExit = 0;
constexpr int kFailedExit = 1;

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

jetsonqt::objectdetection::ObjectDetectionConfig parseConfig(int argc,
                                                             char** argv) {
  jetsonqt::objectdetection::ObjectDetectionConfig config;

  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
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
    } else {
      throw std::runtime_error("Unknown argument: " + argument);
    }
  }

  return config;
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    QGuiApplication app(argc, argv);

    tagvisualizationui::TagVisualizationController controller(
        parseConfig(argc, argv));

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

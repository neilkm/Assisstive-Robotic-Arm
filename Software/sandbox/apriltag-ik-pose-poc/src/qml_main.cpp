#include "PoseController.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

#include <cstdlib>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    QString projectRoot = QCoreApplication::applicationDirPath();
    if (const char* envRoot = std::getenv("APRILTAG_IK_POSE_POC_ROOT")) {
        projectRoot = QString::fromUtf8(envRoot);
    }

    armui::PoseController controller(projectRoot);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("poseController"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/ApriltagIkPose/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}

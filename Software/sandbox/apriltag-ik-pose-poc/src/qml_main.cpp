#include "PoseController.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QUrl>

#include <cstdlib>
#include <QStringList>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    const bool smokeMode = QCoreApplication::arguments().contains(QStringLiteral("--smoke"));

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

    if (smokeMode) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    return QGuiApplication::exec();
}

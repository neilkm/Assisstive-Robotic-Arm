#include "JetsonQtApp/QtUiCommon/AssetPaths.h"
#include "JetsonQtApp/QtUiCommon/Resources.h"
#include "StateMachineUi/StateMachineController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    jetsonqt::qtui::initializeResources();

    QQmlApplicationEngine engine;
    const QString imageDir = jetsonqt::qtui::findExistingAssetDirectory(
        QString::fromLatin1(STATE_MACHINE_UI_ASSET_RELATIVE_PATH));
    statemachineui::StateMachineController controller(imageDir);
    engine.rootContext()->setContextProperty(QStringLiteral("stateMachine"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/StateMachine_UI/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}

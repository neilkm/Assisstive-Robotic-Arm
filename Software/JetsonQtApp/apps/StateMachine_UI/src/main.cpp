#include "JetsonQtApp/QtUiCommon/AssetPaths.h"
#include "StateMachineUi/StateMachineController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QScopedPointer>
#include <QUrl>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QQmlComponent themeComponent(&engine, QUrl(QStringLiteral("qrc:/JetsonQtApp/theme.qml")));
    QScopedPointer<QObject> theme(themeComponent.create());
    if (!theme.isNull()) {
        theme->setParent(&engine);
        engine.rootContext()->setContextProperty(QStringLiteral("theme"), theme.data());
    }

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

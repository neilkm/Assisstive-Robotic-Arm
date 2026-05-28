#include "StateMachineUi/StateMachineController.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

class StateMachineControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void initialStateExposesActionsAndImage();
    void actionSelectionWrapsWithinCurrentState();
    void triggerSelectedActionTransitionsAndEmits();
};

void StateMachineControllerTest::initialStateExposesActionsAndImage() {
    const QTemporaryDir imageDir;
    QVERIFY(imageDir.isValid());

    statemachineui::StateMachineController controller(imageDir.path());

    QCOMPARE(controller.currentStateName(), QStringLiteral("Init"));
    QCOMPARE(controller.selectedActionIndex(), 0);
    QCOMPARE(controller.actions(), QStringList({QStringLiteral("select spice"), QStringLiteral("select utensil")}));
    QVERIFY(controller.imageSource().endsWith(QStringLiteral("/init.png")));
}

void StateMachineControllerTest::actionSelectionWrapsWithinCurrentState() {
    const QTemporaryDir imageDir;
    QVERIFY(imageDir.isValid());

    statemachineui::StateMachineController controller(imageDir.path());

    controller.selectPreviousAction();
    QCOMPARE(controller.selectedActionIndex(), 1);

    controller.selectNextAction();
    QCOMPARE(controller.selectedActionIndex(), 0);
}

void StateMachineControllerTest::triggerSelectedActionTransitionsAndEmits() {
    const QTemporaryDir imageDir;
    QVERIFY(imageDir.isValid());

    statemachineui::StateMachineController controller(imageDir.path());
    QSignalSpy stateChangedSpy(&controller, &statemachineui::StateMachineController::stateChanged);
    QVERIFY(stateChangedSpy.isValid());

    controller.triggerSelectedAction();

    QCOMPARE(controller.currentStateName(), QStringLiteral("Selecting spice"));
    QCOMPARE(controller.selectedActionIndex(), 0);
    QCOMPARE(controller.actions().size(), 9);
    QCOMPARE(stateChangedSpy.count(), 1);
    QVERIFY(controller.imageSource().endsWith(QStringLiteral("/selectspice.png")));
}

}  // namespace

QTEST_GUILESS_MAIN(StateMachineControllerTest)

#include "StateMachineControllerTest.moc"

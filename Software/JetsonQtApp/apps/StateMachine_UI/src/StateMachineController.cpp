#include "StateMachineUi/StateMachineController.h"

#include <QDir>
#include <QUrl>

namespace {

QString toQString(const std::string& value) {
    return QString::fromStdString(value);
}

}  // namespace

namespace statemachineui {

StateMachineController::StateMachineController(QString imageDirectory, QObject* parent)
    : QObject(parent),
      stateMachine_(jetsonqt::statemachine::buildCookingUiStates()),
      imageDirectory_(std::move(imageDirectory)) {
}

QString StateMachineController::currentStateName() const {
    return toQString(stateMachine_.currentState().name);
}

QStringList StateMachineController::actions() const {
    QStringList actionNames;
    for (const std::string& action : stateMachine_.currentState().actions) {
        actionNames.append(toQString(action));
    }

    return actionNames;
}

int StateMachineController::selectedActionIndex() const {
    return static_cast<int>(stateMachine_.selectedActionIndex());
}

int StateMachineController::spiceLevel() const {
    return stateMachine_.spiceLevel();
}

int StateMachineController::stirSpeed() const {
    return stateMachine_.stirSpeed();
}

QString StateMachineController::imageSource() const {
    const QString filename = imageFilenameForState(currentStateName());
    if (filename.isEmpty()) {
        return {};
    }

    return QUrl::fromLocalFile(QDir(imageDirectory_).filePath(filename)).toString();
}

void StateMachineController::selectPreviousAction() {
    stateMachine_.selectPreviousAction();
    emit stateChanged();
}

void StateMachineController::selectNextAction() {
    stateMachine_.selectNextAction();
    emit stateChanged();
}

void StateMachineController::triggerSelectedAction() {
    stateMachine_.triggerSelectedAction();
    emit stateChanged();
}

void StateMachineController::reset() {
    stateMachine_.reset();
    emit stateChanged();
}

QString StateMachineController::imageFilenameForState(const QString& stateName) const {
    if (stateName == QStringLiteral("Init")) {
        return QStringLiteral("init.png");
    }
    if (stateName == QStringLiteral("Selecting spice")) {
        return QStringLiteral("selectspice.png");
    }
    if (stateName == QStringLiteral("Spice selected")) {
        return QStringLiteral("spiceconfirm.png");
    }
    if (stateName == QStringLiteral("Shaking spice into pot")) {
        return QStringLiteral("usingspice.png");
    }
    if (stateName == QStringLiteral("Selecting utensil")) {
        return QStringLiteral("selectutensil.png");
    }
    if (stateName == QStringLiteral("Utensil selected")) {
        return QStringLiteral("confirmutensil.png");
    }
    if (stateName == QStringLiteral("Using utensil")) {
        return QStringLiteral("usingutensil.png");
    }

    return {};
}

}  // namespace statemachineui

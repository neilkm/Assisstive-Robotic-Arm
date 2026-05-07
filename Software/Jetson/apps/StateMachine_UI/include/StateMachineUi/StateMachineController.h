#pragma once

#include "JetsonQtApp/StateMachine/StateMachine.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace statemachineui {

class StateMachineController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentStateName READ currentStateName NOTIFY stateChanged)
    Q_PROPERTY(QStringList actions READ actions NOTIFY stateChanged)
    Q_PROPERTY(int selectedActionIndex READ selectedActionIndex NOTIFY stateChanged)
    Q_PROPERTY(int spiceLevel READ spiceLevel NOTIFY stateChanged)
    Q_PROPERTY(int stirSpeed READ stirSpeed NOTIFY stateChanged)
    Q_PROPERTY(QString imageSource READ imageSource NOTIFY stateChanged)

public:
    explicit StateMachineController(QString imageDirectory, QObject* parent = nullptr);

    [[nodiscard]] QString currentStateName() const;
    [[nodiscard]] QStringList actions() const;
    [[nodiscard]] int selectedActionIndex() const;
    [[nodiscard]] int spiceLevel() const;
    [[nodiscard]] int stirSpeed() const;
    [[nodiscard]] QString imageSource() const;

    Q_INVOKABLE void selectPreviousAction();
    Q_INVOKABLE void selectNextAction();
    Q_INVOKABLE void triggerSelectedAction();
    Q_INVOKABLE void reset();

signals:
    void stateChanged();

private:
    [[nodiscard]] QString imageFilenameForState(const QString& stateName) const;

    jetsonqt::statemachine::StateMachine stateMachine_;
    QString imageDirectory_;
};

}  // namespace statemachineui

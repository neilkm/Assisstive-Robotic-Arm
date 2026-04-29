#pragma once

#include "JetsonQtApp/StateMachine/StateMachine.hpp"

#include <QHash>
#include <QMainWindow>
#include <QPixmap>
#include <QString>

class QLabel;
class QListWidget;
class QListWidgetItem;

namespace statemachineui {

class StateMachineWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit StateMachineWindow(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void loadStateImages();
    void refreshUi();
    void selectPreviousAction();
    void selectNextAction();
    void triggerSelectedAction();
    [[nodiscard]] QString imageFilenameForState(const QString& stateName) const;

    jetsonqt::statemachine::StateMachine stateMachine_;
    QLabel* stateLabel_ = nullptr;
    QListWidget* actionList_ = nullptr;
    QLabel* imageLabel_ = nullptr;
    QHash<QString, QPixmap> stateImages_;
};

}  // namespace statemachineui

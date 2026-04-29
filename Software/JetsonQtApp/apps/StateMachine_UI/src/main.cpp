#include "StateMachineUi/StateMachineWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    statemachineui::StateMachineWindow window;
    window.show();

    return QApplication::exec();
}

jetsonqt_add_library(state_machine_ui
    SOURCES
        src/StateMachineController.cpp
    PUBLIC_HEADERS
        include/StateMachineUi/StateMachineController.h
    PUBLIC_LIBS
        JetsonQtApp::state_machine
        JetsonQtApp::qt_ui_common
        Qt${QT_VERSION_MAJOR}::Multimedia
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Quick
)

target_compile_definitions(state_machine_ui
    PUBLIC
        STATE_MACHINE_UI_ASSET_RELATIVE_PATH="assets/robotarm_state_images"
)

jetsonqt_add_qtest(state_machine_controller_qtest
    SOURCES
        tests/StateMachineControllerTest.cpp
    LIBS
        JetsonQtApp::state_machine_ui
)

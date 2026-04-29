set(STATE_MACHINE_UI_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(STATE_MACHINE_UI_BUILD_ASSET_DIR "$<TARGET_FILE_DIR:StateMachine_UI>/assets")

jetsonqt_add_library(state_machine_ui
    SOURCES
        src/StateMachineWindow.cpp
    PUBLIC_HEADERS
        include/StateMachineUi/StateMachineWindow.hpp
    PUBLIC_LIBS
        JetsonQtApp::state_machine
        JetsonQtApp::qt_ui_common
        Qt${QT_VERSION_MAJOR}::Widgets
)

target_compile_definitions(state_machine_ui
    PRIVATE
        STATE_MACHINE_UI_ASSET_RELATIVE_PATH="assets/robotarm_state_images"
)

jetsonqt_add_app(StateMachine_UI
    SOURCES
        src/main.cpp
    LIBS
        JetsonQtApp::state_machine_ui
)

add_custom_command(TARGET StateMachine_UI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${STATE_MACHINE_UI_ASSET_DIR}"
        "${STATE_MACHINE_UI_BUILD_ASSET_DIR}"
    COMMENT "Copying StateMachine_UI assets"
)

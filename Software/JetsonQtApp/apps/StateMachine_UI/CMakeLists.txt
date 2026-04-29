set(STATE_MACHINE_UI_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(STATE_MACHINE_UI_BUILD_ASSET_DIR "$<TARGET_FILE_DIR:StateMachine_UI>/assets")

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

jetsonqt_add_app(StateMachine_UI
    SOURCES
        src/main.cpp
        src/PluginImports.cpp
        qml/state_machine_ui.qrc
    LIBS
        JetsonQtApp::state_machine_ui
)

if(APPLE)
    if(TARGET Qt${QT_VERSION_MAJOR}::QDarwinCameraPermissionPlugin)
        target_link_libraries(StateMachine_UI PRIVATE Qt${QT_VERSION_MAJOR}::QDarwinCameraPermissionPlugin)
    endif()

    set_target_properties(StateMachine_UI PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in"
    )
endif()

add_custom_command(TARGET StateMachine_UI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${STATE_MACHINE_UI_ASSET_DIR}"
        "${STATE_MACHINE_UI_BUILD_ASSET_DIR}"
    COMMENT "Copying StateMachine_UI assets"
)

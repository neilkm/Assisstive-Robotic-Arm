jetsonqt_add_library(tag_visualization_ui
    SOURCES
        src/TagVisualizationController.cpp
    PUBLIC_HEADERS
        include/TagVisualizationUi/TagVisualizationController.h
    PUBLIC_LIBS
        JetsonQtApp::object_detection
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Quick
)

add_executable(tag_visualization_test
    tests/TagVisualizationTest.cpp
    resources/tag_visualization_test.qrc
)

target_link_libraries(tag_visualization_test
    PRIVATE
        JetsonQtApp::tag_visualization_ui
)

jetsonqt_set_output_directory(tag_visualization_test TEST)

cmake_minimum_required(VERSION 3.22)

project(JetsonQtApp
    VERSION 0.1.0
    DESCRIPTION "Cross-platform Qt applications for the assistive robotic arm"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Gui Multimedia Qml Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Multimedia Qml Quick)

include(cmake/JetsonQtAppHelpers.cmake)

add_subdirectory(libs/state_machine)
add_subdirectory(libs/qt_ui_common)

add_subdirectory(apps/StateMachine_UI)

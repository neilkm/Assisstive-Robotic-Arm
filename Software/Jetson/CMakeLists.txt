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

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Gui Multimedia Qml Quick Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Multimedia Qml Quick Test)

include(cmake/JetsonQtAppHelpers.cmake)

enable_testing()

add_subdirectory(Components/StateMachine)
add_subdirectory(Components/ObjectDetection)
add_subdirectory(Common/UICommon)
add_subdirectory(UserInterface/StateMachine)
add_subdirectory(UserInterface/TagVisualization)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Apps/StateMachine_UI")
    add_subdirectory(Apps/StateMachine_UI)
else()
    add_subdirectory(apps/StateMachine_UI)
endif()

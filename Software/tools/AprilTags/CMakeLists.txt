cmake_minimum_required(VERSION 3.16)
project(AprilTags LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Gui Qml Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Qml Quick)
find_package(OpenCV REQUIRED)

add_executable(AprilTags
    src/AprilTagObject.cpp
    src/AprilTagObject.h
    src/AprilTagProvider.cpp
    src/AprilTagProvider.h
    src/main.cpp
    resources/apriltags.qrc
)

target_include_directories(AprilTags PRIVATE src)

target_link_libraries(AprilTags
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Quick
        ${OpenCV_LIBS}
)

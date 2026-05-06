cmake_minimum_required(VERSION 3.10)
project(apriltag_ik_pose_poc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Gui Qml Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Qml Quick)
find_program(QMLTESTRUNNER_EXECUTABLE NAMES qmltestrunner)

add_library(apriltag_ik_pose_core STATIC
    src/kinematics.cpp
)

target_include_directories(apriltag_ik_pose_core PUBLIC src)

add_executable(apriltag_ik_pose_backend
    src/main.cpp
)

target_link_libraries(apriltag_ik_pose_backend PRIVATE apriltag_ik_pose_core)

add_executable(apriltag_ik_pose_qml
    src/PoseController.cpp
    src/PoseController.hpp
    src/qml_main.cpp
    qml/apriltag_ik_pose_qml.qrc
)

target_link_libraries(apriltag_ik_pose_qml
    PRIVATE
        apriltag_ik_pose_core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Quick
)

add_executable(apriltag_ik_pose_unit_tests
    tests/kinematics_test.cpp
)

target_link_libraries(apriltag_ik_pose_unit_tests PRIVATE apriltag_ik_pose_core)

if(QMLTESTRUNNER_EXECUTABLE)
    add_custom_target(apriltag_ik_pose_qml_tests
        COMMAND ${CMAKE_COMMAND} -E env
            QT_QPA_PLATFORM=offscreen
            "${QMLTESTRUNNER_EXECUTABLE}"
            -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml"
            -import "${CMAKE_CURRENT_SOURCE_DIR}/qml"
        DEPENDS apriltag_ik_pose_qml
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running QML UI tests"
    )
endif()

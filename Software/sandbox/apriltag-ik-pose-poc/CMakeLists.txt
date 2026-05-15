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

# Shared math and compatibility code used by the CLI, QML app, and tests.
add_library(apriltag_ik_pose_core STATIC
    src/AprilTagEndEffectorEstimator.cpp
    src/ArmForwardKinematics.cpp
    src/ArmGeometry.cpp
    src/kinematics.cpp
)

target_include_directories(apriltag_ik_pose_core PUBLIC src)

# Non-GUI CLI for smoke tests and scripted pose/trajectory dumps.
add_executable(apriltag_ik_pose_backend
    src/main.cpp
)

target_link_libraries(apriltag_ik_pose_backend PRIVATE apriltag_ik_pose_core)

# Operator-facing QML visualizer.
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

# Small executable tests keep the sandbox independent from a test framework.
add_executable(apriltag_ik_pose_unit_tests
    tests/kinematics_test.cpp
)

target_link_libraries(apriltag_ik_pose_unit_tests PRIVATE apriltag_ik_pose_core)

add_executable(arm_forward_kinematics_tests
    tests/arm_forward_kinematics_test.cpp
)

target_link_libraries(arm_forward_kinematics_tests PRIVATE apriltag_ik_pose_core)

add_executable(april_tag_end_effector_estimator_tests
    tests/april_tag_end_effector_estimator_test.cpp
)

target_link_libraries(april_tag_end_effector_estimator_tests PRIVATE apriltag_ik_pose_core)

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

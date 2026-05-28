find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio calib3d)

if(OpenCV_VERSION VERSION_GREATER_EQUAL "4.7")
    find_package(OpenCV REQUIRED COMPONENTS objdetect)
    set(OBJECT_DETECTION_APRILTAG_LIB opencv_objdetect)
    set(OBJECT_DETECTION_APRILTAG_DEFINITION OBJECT_DETECTION_USE_OPENCV_OBJDETECT_ARUCO=1)
else()
    find_package(OpenCV REQUIRED COMPONENTS aruco)
    set(OBJECT_DETECTION_APRILTAG_LIB opencv_aruco)
    set(OBJECT_DETECTION_APRILTAG_DEFINITION OBJECT_DETECTION_USE_OPENCV_LEGACY_ARUCO=1)
endif()

jetsonqt_add_library(object_detection
    SOURCES
        src/ObjectDetection.cpp
    PUBLIC_HEADERS
        include/JetsonQtApp/ObjectDetection/ObjectDetection.h
    PUBLIC_INCLUDE_DIRS
        ${OpenCV_INCLUDE_DIRS}
    PUBLIC_LIBS
        ${OBJECT_DETECTION_APRILTAG_LIB}
        opencv_calib3d
        opencv_core
        opencv_imgproc
        opencv_videoio
)

target_compile_definitions(object_detection
    PRIVATE
        ${OBJECT_DETECTION_APRILTAG_DEFINITION}
)

add_executable(object_detection_camera_test
    tests/ObjectDetectionCameraTest.cpp
)

target_link_libraries(object_detection_camera_test
    PRIVATE
        JetsonQtApp::object_detection
)

jetsonqt_set_output_directory(object_detection_camera_test TEST)

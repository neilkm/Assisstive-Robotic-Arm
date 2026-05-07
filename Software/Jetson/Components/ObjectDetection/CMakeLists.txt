find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio calib3d aruco)

jetsonqt_add_library(object_detection
    SOURCES
        src/ObjectDetection.cpp
    PUBLIC_HEADERS
        include/JetsonQtApp/ObjectDetection/ObjectDetection.h
    PUBLIC_INCLUDE_DIRS
        ${OpenCV_INCLUDE_DIRS}
    PUBLIC_LIBS
        opencv_aruco
        opencv_calib3d
        opencv_core
        opencv_imgproc
        opencv_videoio
)

add_executable(object_detection_camera_test
    tests/ObjectDetectionCameraTest.cpp
)

target_link_libraries(object_detection_camera_test
    PRIVATE
        JetsonQtApp::object_detection
)

jetsonqt_set_output_directory(object_detection_camera_test TEST)

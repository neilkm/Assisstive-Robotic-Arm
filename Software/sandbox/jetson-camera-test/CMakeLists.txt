cmake_minimum_required(VERSION 3.10)
project(jetson_camera_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)

add_executable(jetson_camera_test
    src/main.cpp
)

target_include_directories(jetson_camera_test PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(jetson_camera_test PRIVATE ${OpenCV_LIBS})

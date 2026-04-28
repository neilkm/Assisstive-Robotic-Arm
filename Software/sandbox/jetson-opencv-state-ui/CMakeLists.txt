cmake_minimum_required(VERSION 3.10)
project(jetson_opencv_state_ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)

add_executable(jetson_opencv_state_ui
    src/main.cpp
)

target_include_directories(jetson_opencv_state_ui PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(jetson_opencv_state_ui PRIVATE ${OpenCV_LIBS})

cmake_minimum_required(VERSION 3.16)
project(JetsonUartProtocol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(arm_uart_packet INTERFACE)
target_include_directories(arm_uart_packet
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(jetson_uart_driver
    src/uart_driver.cpp
)
target_include_directories(jetson_uart_driver
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(jetson_uart_protocol
    src/main.cpp
)
target_link_libraries(jetson_uart_protocol
    PRIVATE
        arm_uart_packet
        jetson_uart_driver
)


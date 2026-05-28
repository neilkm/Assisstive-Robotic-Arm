cmake_minimum_required(VERSION 3.16)
project(JetsonEsp32BluetoothProtocol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(esp32_button_packet INTERFACE)
target_include_directories(esp32_button_packet
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(jetson_bluetooth_uart_driver
    src/bluetooth_uart_driver.cpp
)
target_include_directories(jetson_bluetooth_uart_driver
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(jetson_esp32_bluetooth_protocol
    src/main.cpp
)
target_link_libraries(jetson_esp32_bluetooth_protocol
    PRIVATE
        esp32_button_packet
        jetson_bluetooth_uart_driver
)


cmake_minimum_required(VERSION 3.16)

project(UART_Driver C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

function(add_uart_driver_test target_name rx_buffer_size tx_buffer_size)
    add_executable(${target_name}
        test.cpp
    )

    target_include_directories(${target_name} PRIVATE
        .
        ../Tests
    )

    target_compile_definitions(${target_name} PRIVATE
        UART_RX_BUF_SIZE=${rx_buffer_size}
        UART_TX_BUF_SIZE=${tx_buffer_size}
    )

    target_link_libraries(${target_name} PRIVATE
        GTest::gtest
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()

    add_test(NAME ${target_name}
        COMMAND ${target_name}
    )
endfunction()

enable_testing()

add_uart_driver_test(uart_driver_test_normal 8 8)
add_uart_driver_test(uart_driver_test_overflow 4 4)
add_uart_driver_test(uart_driver_test_zero 0 0)
add_uart_driver_test(uart_driver_test_maxint 65535 65535)

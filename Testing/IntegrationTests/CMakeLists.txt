set(STM32_INCLUDE_DIR   "${REPO_ROOT}/Software/STM32/include")
set(ESP32_INCLUDE_DIR   "${REPO_ROOT}/Software/ESP32/include")
set(UART_DRIVER_SRC     "${REPO_ROOT}/Software/Jetson/UartProtocol/src/uart_driver.cpp")
set(UART_DRIVER_INC     "${REPO_ROOT}/Software/Jetson/UartProtocol/include")
set(BT_DRIVER_SRC       "${REPO_ROOT}/Software/Jetson/Esp32BluetoothProtocol/src/bluetooth_uart_driver.cpp")
set(BT_DRIVER_INC       "${REPO_ROOT}/Software/Jetson/Esp32BluetoothProtocol/include")
set(COMMON_DIR          "${CMAKE_CURRENT_SOURCE_DIR}/common")

function(add_integration_test target_name)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDES;DEFINES" ${ARGN})

    add_executable(${target_name} ${ARG_SOURCES})

    target_include_directories(${target_name} PRIVATE
        ${COMMON_DIR}
        ${ARG_INCLUDES}
    )

    if(ARG_DEFINES)
        target_compile_definitions(${target_name} PRIVATE ${ARG_DEFINES})
    endif()

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${INTEGRATION_TEST_OUTPUT_DIR}")
endfunction()

# ─── Hardware driver libraries (compiled in regardless; used at runtime when
#     --mode=hardware is passed; no-op in virtual mode) ────────────────────────

add_library(uart_driver_lib STATIC ${UART_DRIVER_SRC})
target_include_directories(uart_driver_lib PUBLIC ${UART_DRIVER_INC})

add_library(bt_driver_lib STATIC ${BT_DRIVER_SRC})
target_include_directories(bt_driver_lib PUBLIC ${BT_DRIVER_INC})

# ─── Shared compile definitions ───────────────────────────────────────────────

set(INTEGRATION_DEFINES "")
if(INTEGRATION_TEST_USE_HARDWARE)
    list(APPEND INTEGRATION_DEFINES INTEGRATION_TEST_USE_HARDWARE)
    message(STATUS "Integration tests: HARDWARE MODE enabled")
    message(STATUS "  UART device : ${INTEGRATION_TEST_UART_DEVICE}")
    message(STATUS "  BT device   : ${INTEGRATION_TEST_BT_DEVICE}")
else()
    message(STATUS "Integration tests: VIRTUAL MODE (socketpair, no hardware required)")
endif()

# ─── UART integration test ────────────────────────────────────────────────────

add_integration_test(uart_integration_test
    SOURCES   uart/uart_integration_test.cpp
    INCLUDES  ${STM32_INCLUDE_DIR} ${UART_DRIVER_INC}
    DEFINES   ${INTEGRATION_DEFINES}
)
if(INTEGRATION_TEST_USE_HARDWARE)
    target_link_libraries(uart_integration_test PRIVATE uart_driver_lib)
    target_compile_definitions(uart_integration_test PRIVATE
        INTEGRATION_TEST_UART_DEVICE="${INTEGRATION_TEST_UART_DEVICE}")
endif()

# ─── Bluetooth integration test ───────────────────────────────────────────────

add_integration_test(bluetooth_integration_test
    SOURCES   bluetooth/bluetooth_integration_test.cpp
    INCLUDES  ${ESP32_INCLUDE_DIR} ${BT_DRIVER_INC}
    DEFINES   ${INTEGRATION_DEFINES}
)
if(INTEGRATION_TEST_USE_HARDWARE)
    target_link_libraries(bluetooth_integration_test PRIVATE bt_driver_lib)
    target_compile_definitions(bluetooth_integration_test PRIVATE
        INTEGRATION_TEST_BT_DEVICE="${INTEGRATION_TEST_BT_DEVICE}")
endif()

# ─── CTest registration (virtual mode only — hardware tests are run manually) ─

if(NOT INTEGRATION_TEST_USE_HARDWARE)
    # UART
    add_test(NAME uart_normal
        COMMAND uart_integration_test --test=normal --iterations=50)
    add_test(NAME uart_drops
        COMMAND uart_integration_test --test=drops --iterations=50 --drop-every=5)
    add_test(NAME uart_duplicates
        COMMAND uart_integration_test --test=duplicates --iterations=50)

    # Bluetooth
    add_test(NAME bt_normal
        COMMAND bluetooth_integration_test --test=normal --iterations=30)
    add_test(NAME bt_drops
        COMMAND bluetooth_integration_test --test=drops --iterations=30 --drop-every=5)
    add_test(NAME bt_duplicates
        COMMAND bluetooth_integration_test --test=duplicates --iterations=30)
    add_test(NAME bt_powercycle
        COMMAND bluetooth_integration_test --test=powercycle --iterations=30 --powercycle-after=15)

    # Label all integration tests so they can be filtered
    set_tests_properties(
        uart_normal uart_drops uart_duplicates
        bt_normal bt_drops bt_duplicates bt_powercycle
        PROPERTIES LABELS "integration")
endif()

set(STM32_INCLUDE_DIR "${REPO_ROOT}/Software/STM32/include")
set(ESP32_INCLUDE_DIR "${REPO_ROOT}/Software/ESP32/include")
set(SM_INCLUDE_DIR    "${REPO_ROOT}/Software/Jetson/QtApp/Components/StateMachine/include")
set(SM_SOURCE         "${REPO_ROOT}/Software/Jetson/QtApp/Components/StateMachine/src/StateMachine.cpp")
set(COMMON_TEST_DIR   "${REPO_ROOT}/Software/Common/Tests")
set(TEST_MAIN_SRC     "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp")

function(add_unit_test target_name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS;INCLUDES" ${ARGN})
    add_executable(${target_name} ${ARG_SOURCES} ${TEST_MAIN_SRC})
    # Link gtest (no main — we supply it via test_main.cpp)
    target_link_libraries(${target_name} PRIVATE GTest::gtest ${ARG_LIBS})
    target_include_directories(${target_name} PRIVATE ${COMMON_TEST_DIR} ${ARG_INCLUDES})
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${UNIT_TEST_OUTPUT_DIR}")
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

# StateMachine compiled as a plain C++ static lib (no Qt dependency)
add_library(state_machine_for_test STATIC ${SM_SOURCE})
target_include_directories(state_machine_for_test PUBLIC ${SM_INCLUDE_DIR})

add_unit_test(arm_uart_packet_unit_test
    SOURCES arm_uart_packet/arm_uart_packet_unit_test.cpp
    INCLUDES ${STM32_INCLUDE_DIR}
)

add_unit_test(esp32_button_packet_unit_test
    SOURCES esp32_button_packet/esp32_button_packet_unit_test.cpp
    INCLUDES ${ESP32_INCLUDE_DIR}
)

add_unit_test(state_machine_unit_test
    SOURCES state_machine/state_machine_unit_test.cpp
    LIBS state_machine_for_test
)

cmake_minimum_required(VERSION 3.16)
project(RoboticArmTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(UNIT_TEST_OUTPUT_DIR "${REPO_ROOT}/builds/UnitTests" CACHE PATH "Unit test output directory")
set(INTEGRATION_TEST_OUTPUT_DIR "${REPO_ROOT}/builds/IntegrationTests" CACHE PATH "Integration test output directory")

option(INTEGRATION_TEST_USE_HARDWARE "Run integration tests against real hardware (OFF = virtual socketpair)" OFF)
set(INTEGRATION_TEST_UART_DEVICE "/dev/ttyACM0" CACHE STRING "UART device path for hardware integration tests")
set(INTEGRATION_TEST_BT_DEVICE   "/dev/rfcomm0" CACHE STRING "Bluetooth device path for hardware integration tests")

enable_testing()

# Prefer a system-installed GTest; fall back to FetchContent if absent.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK  OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    message(STATUS "GTest: using FetchContent v1.17.0")
else()
    message(STATUS "GTest: using system installation (${GTest_VERSION})")
endif()

add_subdirectory(UnitTests)
add_subdirectory(IntegrationTests)

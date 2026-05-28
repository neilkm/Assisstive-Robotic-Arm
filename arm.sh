#!/usr/bin/env bash
# Unified build/flash/test/run script for the assistive robotic arm project.
# Run from the repository root.
#
# Usage: arm.sh <command> [subcommand] [options]
#
# Commands:
#   build jetson  [-qt <path>]          Build Jetson Qt app
#   build stm32   [protocol|echo]       Build STM32 firmware  (default: protocol)
#   build esp32   [normal|echo]         Build ESP32 firmware   (default: normal)
#   build tests   [--hardware]          Build test suite
#   build all                           Build Jetson app + test suite
#
#   flash stm32   [protocol|echo]       Flash STM32 firmware
#   flash esp32   [normal|echo]         Flash ESP32 firmware
#   flash all                           Flash both with production modes
#
#   run           [--ssh]               Run Jetson Qt app (locally or via SSH)
#   test          [--hardware]          Build and run test suite
#
#   clean         [jetson|tests|all]    Remove build directories
#   ssh           [ssh-args...]         Open SSH session to Jetson
#   help                                Show this message

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="${REPO_ROOT}/Software/scripts"
BUILDS_DIR="${REPO_ROOT}/builds"

# ── Production firmware modes ─────────────────────────────────────────────────
# These define what is flashed when using "flash all" or "build all".
# Override by passing an explicit mode argument.
DEFAULT_STM32_MODE="protocol"
DEFAULT_ESP32_MODE="normal"

# ── PlatformIO environment names ──────────────────────────────────────────────
stm32_env() {
    case "${1:-${DEFAULT_STM32_MODE}}" in
        protocol) echo "nucleo_f446re_uart_protocol" ;;
        echo)     echo "nucleo_f446re" ;;
        *) echo "error: unknown STM32 mode '${1}' (use: protocol, echo)" >&2; exit 1 ;;
    esac
}

esp32_env() {
    case "${1:-${DEFAULT_ESP32_MODE}}" in
        normal) echo "esp32dev" ;;
        echo)   echo "esp32dev_bluetooth_echo_test" ;;
        *) echo "error: unknown ESP32 mode '${1}' (use: normal, echo)" >&2; exit 1 ;;
    esac
}

# ── Helpers ───────────────────────────────────────────────────────────────────
die()  { echo "error: $*" >&2; exit 1; }
info() { echo "==> $*"; }

require_pio() {
    command -v pio >/dev/null 2>&1 || die "pio not found — install PlatformIO: pip install platformio"
}

require_cmake() {
    command -v cmake >/dev/null 2>&1 || die "cmake not found"
}

# ── build ─────────────────────────────────────────────────────────────────────
cmd_build() {
    local target="${1:-}" ; shift || true
    case "${target}" in
        jetson) build_jetson "$@" ;;
        stm32)  build_stm32  "$@" ;;
        esp32)  build_esp32  "$@" ;;
        tests)  build_tests  "$@" ;;
        all)
            build_jetson "$@"
            build_tests
            ;;
        "") die "build requires a target. Run: arm.sh help" ;;
        *)  die "unknown build target '${target}'. Run: arm.sh help" ;;
    esac
}

build_jetson() {
    require_cmake
    local cmake_args=()
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -qt) cmake_args+=("-DCMAKE_PREFIX_PATH=${2}"); shift 2 ;;
            *)   cmake_args+=("$1"); shift ;;
        esac
    done

    info "Building Jetson Qt app"
    cmake -S "${REPO_ROOT}/Software/Jetson/QtApp" \
          -B "${BUILDS_DIR}/Jetson" \
          -G Ninja \
          "${cmake_args[@]+"${cmake_args[@]}"}"
    cmake --build "${BUILDS_DIR}/Jetson"
}

build_stm32() {
    require_pio
    local mode="${1:-${DEFAULT_STM32_MODE}}"
    local env; env="$(stm32_env "${mode}")"
    info "Building STM32 firmware: ${env} (mode=${mode})"
    pio run -d "${REPO_ROOT}/Software/STM32" -e "${env}"
}

build_esp32() {
    require_pio
    local mode="${1:-${DEFAULT_ESP32_MODE}}"
    local env; env="$(esp32_env "${mode}")"
    info "Building ESP32 firmware: ${env} (mode=${mode})"
    pio run -d "${REPO_ROOT}/Software/ESP32" -e "${env}"
}

build_tests() {
    require_cmake
    local use_hardware=OFF
    local extra_args=()
    for arg in "$@"; do
        case "${arg}" in
            --hardware) use_hardware=ON ;;
            *)          extra_args+=("${arg}") ;;
        esac
    done

    info "Building test suite (hardware=${use_hardware})"
    cmake -S "${REPO_ROOT}/Testing" \
          -B "${BUILDS_DIR}/Testing" \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DINTEGRATION_TEST_USE_HARDWARE="${use_hardware}" \
          "${extra_args[@]+"${extra_args[@]}"}"
    cmake --build "${BUILDS_DIR}/Testing"
}

# ── flash ─────────────────────────────────────────────────────────────────────
cmd_flash() {
    local target="${1:-}" ; shift || true
    case "${target}" in
        stm32) flash_stm32 "$@" ;;
        esp32) flash_esp32 "$@" ;;
        all)
            flash_stm32 "${DEFAULT_STM32_MODE}" "$@"
            flash_esp32 "${DEFAULT_ESP32_MODE}" "$@"
            ;;
        "") die "flash requires a target. Run: arm.sh help" ;;
        *)  die "unknown flash target '${target}'. Run: arm.sh help" ;;
    esac
}

flash_stm32() {
    require_pio
    local mode="${1:-${DEFAULT_STM32_MODE}}"; shift || true
    local env; env="$(stm32_env "${mode}")"
    info "Flashing STM32: ${env} (mode=${mode})"
    pio run -d "${REPO_ROOT}/Software/STM32" -e "${env}" -t upload "$@"
}

flash_esp32() {
    require_pio
    local mode="${1:-${DEFAULT_ESP32_MODE}}"; shift || true
    local env; env="$(esp32_env "${mode}")"
    info "Flashing ESP32: ${env} (mode=${mode})"
    pio run -d "${REPO_ROOT}/Software/ESP32" -e "${env}" -t upload "$@"
}

# ── run ───────────────────────────────────────────────────────────────────────
cmd_run() {
    exec "${SCRIPTS_DIR}/run_jetson_qt_state_machine_ui.sh" "$@"
}

# ── test ──────────────────────────────────────────────────────────────────────
cmd_test() {
    build_tests "$@"

    local use_hardware=OFF
    for arg in "$@"; do
        [[ "${arg}" == "--hardware" ]] && use_hardware=ON
    done

    info "Running tests (hardware=${use_hardware})"
    local timeout=120
    [[ "${use_hardware}" == "ON" ]] && timeout=300
    ctest --test-dir "${BUILDS_DIR}/Testing" --output-on-failure --timeout "${timeout}"
}

# ── clean ─────────────────────────────────────────────────────────────────────
cmd_clean() {
    local target="${1:-all}"
    case "${target}" in
        jetson) cmake -E rm -rf "${BUILDS_DIR}/Jetson" ;;
        tests)  cmake -E rm -rf "${BUILDS_DIR}/Testing" ;;
        all)
            cmake -E rm -rf \
                "${BUILDS_DIR}/Jetson" \
                "${BUILDS_DIR}/Testing" \
                "${BUILDS_DIR}/Apps" \
                "${BUILDS_DIR}/UnitTests" \
                "${BUILDS_DIR}/IntegrationTests"
            ;;
        *) die "unknown clean target '${target}' (use: jetson, tests, all)" ;;
    esac
    info "Cleaned: ${target}"
}

# ── ssh ───────────────────────────────────────────────────────────────────────
cmd_ssh() {
    exec "${SCRIPTS_DIR}/ssh_jetson.sh" "$@"
}

# ── help ──────────────────────────────────────────────────────────────────────
cmd_help() {
    cat <<'EOF'
arm.sh — assistive robotic arm build tool

USAGE
  arm.sh <command> [subcommand] [options]

BUILD
  arm.sh build jetson [-qt <qt-prefix>]   Build Jetson Qt app
  arm.sh build stm32  [protocol|echo]     Build STM32 firmware  (default: protocol)
  arm.sh build esp32  [normal|echo]       Build ESP32 firmware   (default: normal)
  arm.sh build tests  [--hardware]        Build test suite
  arm.sh build all                        Build Jetson app + test suite

FLASH
  arm.sh flash stm32  [protocol|echo]     Flash STM32 firmware
  arm.sh flash esp32  [normal|echo]       Flash ESP32 firmware
  arm.sh flash all                        Flash both with production modes

RUN / TEST
  arm.sh run                              Run Jetson Qt app
  arm.sh test [--hardware]                Build and run test suite

UTILITIES
  arm.sh clean [jetson|tests|all]         Remove build directories
  arm.sh ssh   [ssh-args...]              Open SSH session to Jetson
  arm.sh help                             Show this message

FIRMWARE MODES
  STM32  protocol  nucleo_f446re_uart_protocol   production UART protocol
         echo      nucleo_f446re                 text echo test
  ESP32  normal    esp32dev                      random button telemetry
         echo      esp32dev_bluetooth_echo_test  Bluetooth UART echo test

  "arm.sh flash all" uses the default production modes (protocol + normal).

EXAMPLES
  arm.sh build all
  arm.sh flash stm32 protocol
  arm.sh flash esp32 normal
  arm.sh test
  arm.sh test --hardware
  arm.sh build jetson -qt "$(brew --prefix qt)"
  arm.sh clean all
EOF
}

# ── dispatch ──────────────────────────────────────────────────────────────────
case "${1:-help}" in
    build) shift; cmd_build "$@" ;;
    flash) shift; cmd_flash "$@" ;;
    run)   shift; cmd_run   "$@" ;;
    test)  shift; cmd_test  "$@" ;;
    clean) shift; cmd_clean "$@" ;;
    ssh)   shift; cmd_ssh   "$@" ;;
    help|--help|-h) cmd_help ;;
    *) echo "error: unknown command '${1}'" >&2; cmd_help; exit 1 ;;
esac

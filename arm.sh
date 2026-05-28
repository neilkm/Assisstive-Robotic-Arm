#!/usr/bin/env bash
# Unified build/flash/test/run/ssh script for the assistive robotic arm project.
# Run from the repository root.  Run "arm.sh help" for full usage.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILDS_DIR="${REPO_ROOT}/builds"

# ── Production firmware modes ─────────────────────────────────────────────────
DEFAULT_STM32_MODE="protocol"
DEFAULT_ESP32_MODE="normal"

# ── PlatformIO environment names ──────────────────────────────────────────────
stm32_env() {
    case "${1:-${DEFAULT_STM32_MODE}}" in
        protocol) echo "nucleo_f446re_uart_protocol" ;;
        echo)     echo "nucleo_f446re" ;;
        *) die "unknown STM32 mode '${1}' (use: protocol, echo)" ;;
    esac
}

esp32_env() {
    case "${1:-${DEFAULT_ESP32_MODE}}" in
        normal) echo "esp32dev" ;;
        echo)   echo "esp32dev_bluetooth_echo_test" ;;
        *) die "unknown ESP32 mode '${1}' (use: normal, echo)" ;;
    esac
}

# ── Helpers ───────────────────────────────────────────────────────────────────
die()  { echo "error: $*" >&2; exit 1; }
info() { echo "==> $*"; }
require() { command -v "$1" >/dev/null 2>&1 || die "$1 not found — $2"; }

# ── build ─────────────────────────────────────────────────────────────────────
cmd_build() {
    local target="${1:-}"; shift || true
    case "${target}" in
        jetson) build_jetson "$@" ;;
        stm32)  build_stm32  "$@" ;;
        esp32)  build_esp32  "$@" ;;
        tests)  build_tests  "$@" ;;
        all)    build_jetson "$@"; build_tests ;;
        "")     die "build requires a target. Run: arm.sh help" ;;
        *)      die "unknown build target '${target}'. Run: arm.sh help" ;;
    esac
}

build_jetson() {
    require cmake "install Qt + cmake first"
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
    require pio "install PlatformIO: pip install platformio"
    local mode="${1:-${DEFAULT_STM32_MODE}}"
    local env; env="$(stm32_env "${mode}")"
    info "Building STM32 firmware: ${env}"
    pio run -d "${REPO_ROOT}/Software/STM32" -e "${env}"
}

build_esp32() {
    require pio "install PlatformIO: pip install platformio"
    local mode="${1:-${DEFAULT_ESP32_MODE}}"
    local env; env="$(esp32_env "${mode}")"
    info "Building ESP32 firmware: ${env}"
    pio run -d "${REPO_ROOT}/Software/ESP32" -e "${env}"
}

build_tests() {
    require cmake "install cmake first"
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
    local target="${1:-}"; shift || true
    case "${target}" in
        stm32) flash_stm32 "$@" ;;
        esp32) flash_esp32 "$@" ;;
        all)   flash_stm32 "${DEFAULT_STM32_MODE}"; flash_esp32 "${DEFAULT_ESP32_MODE}" ;;
        "")    die "flash requires a target. Run: arm.sh help" ;;
        *)     die "unknown flash target '${target}'. Run: arm.sh help" ;;
    esac
}

flash_stm32() {
    require pio "install PlatformIO: pip install platformio"
    local mode="${1:-${DEFAULT_STM32_MODE}}"; shift || true
    local env; env="$(stm32_env "${mode}")"
    info "Flashing STM32: ${env}"
    pio run -d "${REPO_ROOT}/Software/STM32" -e "${env}" -t upload "$@"
}

flash_esp32() {
    require pio "install PlatformIO: pip install platformio"
    local mode="${1:-${DEFAULT_ESP32_MODE}}"; shift || true
    local env; env="$(esp32_env "${mode}")"
    info "Flashing ESP32: ${env}"
    pio run -d "${REPO_ROOT}/Software/ESP32" -e "${env}" -t upload "$@"
}

# ── run ───────────────────────────────────────────────────────────────────────
cmd_run() {
    local app="${BUILDS_DIR}/Apps/StateMachine_UI/StateMachine_UI"
    local mac_app="${BUILDS_DIR}/Apps/StateMachine_UI/StateMachine_UI.app/Contents/MacOS/StateMachine_UI"

    [[ -x "${mac_app}" ]] && exec "${mac_app}"

    if [[ "$(uname -s)" == "Linux" ]]; then
        _configure_jetson_display
    fi

    if [[ ! -x "${app}" && ! -x "${mac_app}" ]]; then
        info "App not built — building first"
        build_jetson "$@"
    fi

    [[ -x "${mac_app}" ]] && exec "${mac_app}"
    exec "${app}"
}

_configure_jetson_display() {
    local runtime_dir="/run/user/$(id -u)"
    [[ -z "${XDG_RUNTIME_DIR:-}" && -d "${runtime_dir}" ]] && export XDG_RUNTIME_DIR="${runtime_dir}"

    if [[ -n "${JETSON_QT_DISPLAY:-}" ]]; then
        export DISPLAY="${JETSON_QT_DISPLAY}"
        export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
    elif [[ -z "${QT_QPA_PLATFORM:-}" ]]; then
        local sock
        for sock in "${XDG_RUNTIME_DIR:-}"/wayland-*; do
            if [[ -S "${sock}" ]]; then
                export WAYLAND_DISPLAY="$(basename "${sock}")"
                export QT_QPA_PLATFORM="wayland"
                break
            fi
        done
        if [[ -z "${QT_QPA_PLATFORM:-}" ]]; then
            for sock in /tmp/.X11-unix/X*; do
                if [[ -S "${sock}" ]]; then
                    export DISPLAY=":${sock##*X}"
                    export QT_QPA_PLATFORM="xcb"
                    break
                fi
            done
        fi
        if [[ -z "${QT_QPA_PLATFORM:-}" ]]; then
            export DISPLAY=":0"
            export QT_QPA_PLATFORM="xcb"
        fi
    fi
    if [[ -z "${XAUTHORITY:-}" ]]; then
        local cand
        for cand in "${HOME}/.Xauthority" \
                    "${XDG_RUNTIME_DIR:-}/gdm/Xauthority" \
                    "${XDG_RUNTIME_DIR:-}/Xauthority"; do
            [[ -f "${cand}" ]] && export XAUTHORITY="${cand}" && break
        done
    fi
}

# ── test ──────────────────────────────────────────────────────────────────────
cmd_test() {
    build_tests "$@"
    local use_hardware=OFF
    for arg in "$@"; do [[ "${arg}" == "--hardware" ]] && use_hardware=ON; done
    local timeout=120
    [[ "${use_hardware}" == "ON" ]] && timeout=300
    info "Running tests (hardware=${use_hardware})"
    ctest --test-dir "${BUILDS_DIR}/Testing" --output-on-failure --timeout "${timeout}"
}

# ── sandbox ───────────────────────────────────────────────────────────────────
cmd_sandbox() {
    local subcmd="${1:-}"; shift || true
    case "${subcmd}" in
        build) sandbox_build "$@" ;;
        run)   sandbox_run   "$@" ;;
        "")    die "sandbox requires build or run. Run: arm.sh help" ;;
        *)     die "unknown sandbox subcommand '${subcmd}'" ;;
    esac
}

_sandbox_cmake_build() {
    local name="$1" src="$2"
    local build_dir="${BUILDS_DIR}/sandbox/${name}"
    require cmake "install cmake first"
    info "Building sandbox/${name}"
    cmake -S "${src}" -B "${build_dir}" -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build "${build_dir}"
}

_sandbox_python_setup() {
    local name="$1" src="$2"
    info "Setting up sandbox/${name} Python environment"
    local venv="${src}/.venv"
    python3 -m venv "${venv}"
    "${venv}/bin/pip" install --upgrade pip -q
    "${venv}/bin/pip" install -r "${src}/requirements.txt" -q
    info "Done — run: arm.sh sandbox run ${name}"
}

sandbox_build() {
    local target="${1:-all}"; shift || true
    case "${target}" in
        apriltag-ik)
            _sandbox_cmake_build apriltag-ik "${REPO_ROOT}/Software/sandbox/apriltag-ik-pose-poc" ;;
        camera-test)
            _sandbox_cmake_build camera-test "${REPO_ROOT}/Software/sandbox/jetson-camera-test" ;;
        opencv-state-ui)
            _sandbox_cmake_build opencv-state-ui "${REPO_ROOT}/Software/sandbox/jetson-opencv-state-ui" ;;
        apriltag-detector)
            _sandbox_python_setup apriltag-detector "${REPO_ROOT}/Software/sandbox/AprilTag_PoseDetector" ;;
        robot-sim)
            _sandbox_python_setup robot-sim "${REPO_ROOT}/Software/sandbox/robot-arm-3d-sim" ;;
        all)
            _sandbox_cmake_build apriltag-ik "${REPO_ROOT}/Software/sandbox/apriltag-ik-pose-poc"
            _sandbox_cmake_build camera-test "${REPO_ROOT}/Software/sandbox/jetson-camera-test"
            _sandbox_cmake_build opencv-state-ui "${REPO_ROOT}/Software/sandbox/jetson-opencv-state-ui"
            _sandbox_python_setup apriltag-detector "${REPO_ROOT}/Software/sandbox/AprilTag_PoseDetector"
            _sandbox_python_setup robot-sim "${REPO_ROOT}/Software/sandbox/robot-arm-3d-sim"
            ;;
        *) die "unknown sandbox app '${target}'. Use: apriltag-ik, camera-test, opencv-state-ui, apriltag-detector, robot-sim, all" ;;
    esac
}

sandbox_run() {
    local target="${1:-}"; shift || true
    local build_dir="${BUILDS_DIR}/sandbox"
    case "${target}" in
        apriltag-ik)
            exec "${build_dir}/apriltag-ik/apriltag_ik_pose_qml" "$@" ;;
        apriltag-ik-backend)
            exec "${build_dir}/apriltag-ik/apriltag_ik_pose_backend" "$@" ;;
        camera-test)
            exec "${build_dir}/camera-test/jetson_camera_test" "$@" ;;
        opencv-state-ui)
            exec "${build_dir}/opencv-state-ui/jetson_opencv_state_ui" "$@" ;;
        apriltag-detector)
            local src="${REPO_ROOT}/Software/sandbox/AprilTag_PoseDetector"
            exec "${src}/.venv/bin/python" "${src}/src/detect_pose.py" "$@" ;;
        robot-sim)
            local src="${REPO_ROOT}/Software/sandbox/robot-arm-3d-sim"
            local mode="${1:-gui}"; shift || true
            if [[ "${mode}" == "cli" ]]; then
                exec "${src}/.venv/bin/python" "${src}/src/kinematics.py" "$@"
            else
                exec "${src}/.venv/bin/python" "${src}/src/xyz_gui.py" "$@"
            fi
            ;;
        "") die "sandbox run requires an app name. Run: arm.sh help" ;;
        *)  die "unknown sandbox app '${target}'. Run: arm.sh help" ;;
    esac
}

# ── tools ─────────────────────────────────────────────────────────────────────
cmd_tools() {
    local subcmd="${1:-}"; shift || true
    case "${subcmd}" in
        run) tools_run "$@" ;;
        "")  die "tools requires a subcommand. Run: arm.sh help" ;;
        *)   die "unknown tools subcommand '${subcmd}'" ;;
    esac
}

tools_run() {
    local tool="${1:-}"; shift || true
    case "${tool}" in
        apriltag-pdf)
            require python3 "install python3 first"
            exec python3 "${REPO_ROOT}/Tools/Mac/AprilTags/generate_apriltag_pdf.py" "$@" ;;
        jetson-burn)
            exec "${REPO_ROOT}/Tools/Jetson/PowerDrawTest/jetson-burn.sh" "$@" ;;
        system-monitor)
            exec "${REPO_ROOT}/Tools/Jetson/SystemMonitor/System-Monitor-NNK" "$@" ;;
        "") die "tools run requires a tool name. Run: arm.sh help" ;;
        *)  die "unknown tool '${tool}'. Run: arm.sh help" ;;
    esac
}

# ── setup ─────────────────────────────────────────────────────────────────────
cmd_setup() {
    [[ "$(uname -s)" == "Linux" ]] || die "setup is for Jetson Ubuntu only (detected: $(uname -s))"
    info "Installing Jetson Qt + PlatformIO dependencies"
    sudo apt-get update
    sudo apt-get install -y \
        build-essential cmake ninja-build python3-pip python3-venv pkg-config \
        libxkbcommon-dev libxcb-cursor0 libxcb-icccm4 libxcb-image0 \
        libxcb-keysyms1 libxcb-render-util0 libxcb-xinerama0 \
        qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qt6-wayland \
        qml6-module-qtqml qml6-module-qtqml-workerscript \
        qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtmultimedia \
        || sudo apt-get install -y \
        qtbase5-dev qtmultimedia5-dev qtdeclarative5-dev \
        qml-module-qtquick2 qml-module-qtquick-controls2 qml-module-qtqml-workerscript

    python3 -m pip install --user --upgrade platformio

    local user="${SUDO_USER:-${USER}}"
    if getent group dialout >/dev/null 2>&1; then
        sudo usermod -a -G dialout "${user}"
        echo ""
        echo "Added ${user} to dialout group — log out and back in before flashing MCUs."
    fi

    if ! command -v pio >/dev/null 2>&1; then
        echo ""
        echo "PlatformIO installed. Add to PATH if not found:"
        echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
    fi
}

# ── clean ─────────────────────────────────────────────────────────────────────
cmd_clean() {
    local target="${1:-all}"
    case "${target}" in
        jetson)  cmake -E rm -rf "${BUILDS_DIR}/Jetson" "${BUILDS_DIR}/Apps" "${BUILDS_DIR}/UnitTests" ;;
        tests)   cmake -E rm -rf "${BUILDS_DIR}/Testing" "${BUILDS_DIR}/IntegrationTests" ;;
        sandbox) cmake -E rm -rf "${BUILDS_DIR}/sandbox" ;;
        all)     cmake -E rm -rf "${BUILDS_DIR}" ;;
        *) die "unknown clean target '${target}' (use: jetson, tests, sandbox, all)" ;;
    esac
    info "Cleaned: ${target}"
}

# ── ssh ───────────────────────────────────────────────────────────────────────
cmd_ssh() {
    exec ssh jetson "$@"
}

# ── help ──────────────────────────────────────────────────────────────────────
cmd_help() {
    cat <<'EOF'
arm.sh — assistive robotic arm build tool

USAGE
  arm.sh <command> [subcommand] [options]

BUILD
  arm.sh build jetson  [-qt <qt-prefix>]   Build Jetson Qt app
  arm.sh build stm32   [protocol|echo]     Build STM32 firmware  (default: protocol)
  arm.sh build esp32   [normal|echo]       Build ESP32 firmware   (default: normal)
  arm.sh build tests   [--hardware]        Build test suite
  arm.sh build all                         Build Jetson app + test suite

FLASH
  arm.sh flash stm32   [protocol|echo]     Flash STM32 firmware
  arm.sh flash esp32   [normal|echo]       Flash ESP32 firmware
  arm.sh flash all                         Flash both with production modes

RUN / TEST
  arm.sh run                               Run Jetson Qt app (builds if needed)
  arm.sh test [--hardware]                 Build and run test suite

SANDBOX
  arm.sh sandbox build [<app>|all]         Build a sandbox app
  arm.sh sandbox run   <app> [args...]     Run a sandbox app

  Sandbox apps: apriltag-ik, apriltag-ik-backend, camera-test,
                opencv-state-ui, apriltag-detector, robot-sim

TOOLS
  arm.sh tools run apriltag-pdf [args...]  Generate AprilTag PDF (Mac)
  arm.sh tools run jetson-burn             Run Jetson CUDA power-draw test
  arm.sh tools run system-monitor          Run Jetson system monitor TUI

UTILITIES
  arm.sh setup                             Install Jetson build dependencies (Linux)
  arm.sh clean  [jetson|tests|sandbox|all] Remove build directories
  arm.sh ssh    [ssh-args...]              SSH to Jetson
  arm.sh help                              Show this message

FIRMWARE MODES
  STM32  protocol  nucleo_f446re_uart_protocol   production UART protocol
         echo      nucleo_f446re                 text echo test
  ESP32  normal    esp32dev                      random button telemetry
         echo      esp32dev_bluetooth_echo_test  Bluetooth UART echo test

EXAMPLES
  arm.sh build all
  arm.sh flash all
  arm.sh test
  arm.sh test --hardware
  arm.sh sandbox build apriltag-ik
  arm.sh sandbox run apriltag-ik
  arm.sh tools run apriltag-pdf --family tag36h11 --id 0
  arm.sh build jetson -qt "$(brew --prefix qt)"
  arm.sh clean all
EOF
}

# ── dispatch ──────────────────────────────────────────────────────────────────
case "${1:-help}" in
    build)   shift; cmd_build   "$@" ;;
    flash)   shift; cmd_flash   "$@" ;;
    run)     shift; cmd_run     "$@" ;;
    test)    shift; cmd_test    "$@" ;;
    sandbox) shift; cmd_sandbox "$@" ;;
    tools)   shift; cmd_tools   "$@" ;;
    setup)   shift; cmd_setup   "$@" ;;
    clean)   shift; cmd_clean   "$@" ;;
    ssh)     shift; cmd_ssh     "$@" ;;
    help|--help|-h) cmd_help ;;
    *) echo "error: unknown command '${1}'" >&2; cmd_help; exit 1 ;;
esac

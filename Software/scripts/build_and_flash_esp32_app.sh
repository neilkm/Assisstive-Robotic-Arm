#!/usr/bin/env bash
# Build and flash an ESP32 PlatformIO environment.
#
# Usage: build_and_flash_esp32_app.sh [normal|echo] [pio-args...]
#
#   normal   esp32dev                    random button telemetry (default)
#   echo     esp32dev_bluetooth_echo_test  Bluetooth UART echo test

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/../ESP32" && pwd)"

MODE="${1:-normal}"; shift || true
case "${MODE}" in
    normal) ENV="esp32dev" ;;
    echo)   ENV="esp32dev_bluetooth_echo_test" ;;
    *)
        echo "error: unknown mode '${MODE}' (use: normal, echo)" >&2
        exit 1
        ;;
esac

pio run -d "${PROJECT_DIR}" -e "${ENV}" "$@"
pio run -d "${PROJECT_DIR}" -e "${ENV}" -t upload "$@"

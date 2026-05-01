#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/../ESP32App" && pwd)"

cd "${PROJECT_DIR}"

pio run "$@"
pio run -t upload "$@"

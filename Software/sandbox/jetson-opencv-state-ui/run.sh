#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BINARY="${BUILD_DIR}/jetson_opencv_state_ui"

if [[ ! -x "${BINARY}" ]]; then
  echo "Binary not found. Building first."
  "${SCRIPT_DIR}/build.sh"
fi

"${BINARY}"

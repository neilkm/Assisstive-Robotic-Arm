#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/../Jetson" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/builds/Jetson"

cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" -G Ninja "$@"
cmake --build "${BUILD_DIR}"

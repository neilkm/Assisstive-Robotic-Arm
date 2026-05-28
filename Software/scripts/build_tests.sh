#!/usr/bin/env bash
# Build and optionally run the test suite.
#
# Usage:
#   build_tests.sh [--run] [--hardware] [cmake extra args...]
#
#   --run       also run all tests with ctest after building
#   --hardware  enable hardware integration tests (requires real devices)
#
# Examples:
#   build_tests.sh                         # build only (virtual mode)
#   build_tests.sh --run                   # build + run virtual tests
#   build_tests.sh --hardware --run        # build + run hardware tests
#   build_tests.sh --run -DCMAKE_BUILD_TYPE=Debug

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/builds/Testing"

USE_HARDWARE=OFF
RUN_TESTS=false
EXTRA_ARGS=()

for arg in "$@"; do
    case "${arg}" in
        --run)      RUN_TESTS=true ;;
        --hardware) USE_HARDWARE=ON ;;
        *)          EXTRA_ARGS+=("${arg}") ;;
    esac
done

echo ""
echo "============================================================"
echo "  Robotic Arm Test Suite"
echo "  hardware mode : ${USE_HARDWARE}"
echo "  run after build: ${RUN_TESTS}"
echo "============================================================"
echo ""

cmake -S "${REPO_ROOT}/Testing" \
      -B "${BUILD_DIR}" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DINTEGRATION_TEST_USE_HARDWARE="${USE_HARDWARE}" \
      "${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}"

cmake --build "${BUILD_DIR}"

echo ""
echo "Build output:"
echo "  Unit tests      -> ${REPO_ROOT}/builds/UnitTests/"
echo "  Integration     -> ${REPO_ROOT}/builds/IntegrationTests/"
echo ""

if [[ "${RUN_TESTS}" == "true" ]]; then
    echo "Running tests..."
    if [[ "${USE_HARDWARE}" == "ON" ]]; then
        ctest --test-dir "${BUILD_DIR}" --output-on-failure --timeout 300
    else
        ctest --test-dir "${BUILD_DIR}" --output-on-failure --timeout 120
    fi
fi

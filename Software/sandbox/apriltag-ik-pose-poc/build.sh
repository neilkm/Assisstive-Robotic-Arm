#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
JOBS="${JOBS:-}"

if [[ -z "${JOBS}" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
  elif command -v getconf >/dev/null 2>&1; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
  elif command -v sysctl >/dev/null 2>&1; then
    JOBS="$(sysctl -n hw.ncpu 2>/dev/null || true)"
  else
    JOBS=1
  fi
  if [[ -z "${JOBS}" ]]; then
    JOBS=1
  fi
fi

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" -- -j"${JOBS}"

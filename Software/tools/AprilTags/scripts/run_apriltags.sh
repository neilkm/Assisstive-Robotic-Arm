#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$PROJECT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/builds/tools/AprilTags"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR"

cd "$BUILD_DIR"
exec "$BUILD_DIR/AprilTags" "$@"

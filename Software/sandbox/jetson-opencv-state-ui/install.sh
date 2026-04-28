#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "== jetson-opencv-state-ui =="
echo "Installing system dependencies for Jetson Nano / Ubuntu"

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  pkg-config \
  libopencv-dev

echo "== Building project =="
"${SCRIPT_DIR}/build.sh"

echo "Install complete."
echo "Run with: ${SCRIPT_DIR}/run.sh"

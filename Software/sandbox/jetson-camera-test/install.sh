#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "== jetson-camera-test =="
echo "Installing dependencies for Jetson Nano / Ubuntu"

if ! command -v apt-get >/dev/null 2>&1; then
  echo "This installer targets Ubuntu-based systems with apt-get."
  echo "Install OpenCV and CMake manually on this host, then run ${SCRIPT_DIR}/build.sh"
  exit 1
fi

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  pkg-config \
  libopencv-dev \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-tools \
  v4l-utils

if command -v gst-inspect-1.0 >/dev/null 2>&1; then
  if ! gst-inspect-1.0 nvarguscamerasrc >/dev/null 2>&1; then
    echo "Warning: nvarguscamerasrc was not found."
    echo "USB /dev/video* cameras will still work."
    echo "CSI cameras require the NVIDIA JetPack camera stack on Jetson Nano."
  fi
fi

echo "== Building project =="
"${SCRIPT_DIR}/build.sh"

echo "Install complete."
echo "Run with: ${SCRIPT_DIR}/run.sh"

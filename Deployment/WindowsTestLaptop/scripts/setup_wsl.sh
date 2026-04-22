#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cargo \
  cmake \
  gcc-arm-none-eabi \
  gdb-multiarch \
  git \
  libusb-1.0-0-dev \
  make \
  ninja-build \
  openocd \
  pkg-config \
  python3 \
  python3-dev \
  python3-pip \
  python3-venv \
  stlink-tools \
  usbutils

python3 -m venv "$REPO_ROOT/.venv"
"$REPO_ROOT/.venv/bin/python" -m pip install --upgrade pip
"$REPO_ROOT/.venv/bin/python" -m pip install -r "$REPO_ROOT/Deployment/WindowsTestLaptop/requirements.txt"

sudo usermod -aG dialout "$USER"

echo "WSL setup complete."
echo "Restart WSL or log out and back in so dialout group membership takes effect."
echo "Next: copy config/hwci.example.yaml to config/hwci.yaml and edit device commands."


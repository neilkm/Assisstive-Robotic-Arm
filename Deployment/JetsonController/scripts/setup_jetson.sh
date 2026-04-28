#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
UDEV_RULES="/etc/udev/rules.d/99-assistive-arm.rules"

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
  openssh-server \
  pkg-config \
  python3 \
  python3-dev \
  python3-pip \
  python3-venv \
  stlink-tools \
  udev \
  usbutils

python3 -m venv "$REPO_ROOT/.venv"
"$REPO_ROOT/.venv/bin/python" -m pip install --upgrade pip
"$REPO_ROOT/.venv/bin/python" -m pip install -r "$REPO_ROOT/Deployment/JetsonController/requirements.txt"

sudo usermod -aG dialout,plugdev "$USER"
sudo systemctl enable --now ssh

if [ ! -f "$UDEV_RULES" ]; then
  sudo tee "$UDEV_RULES" >/dev/null <<'RULES'
# Assistive robotic arm hardware CI device aliases.
# Fill in idVendor/idProduct and serial values after checking `udevadm info`.
#
# Example STM32 ST-LINK VCP alias:
# SUBSYSTEM=="tty", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", SYMLINK+="assistive_stm32", GROUP="dialout", MODE="0660"
#
# Example ESP32 USB-UART alias:
# SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="assistive_esp32", GROUP="dialout", MODE="0660"
RULES
fi

sudo udevadm control --reload-rules
sudo udevadm trigger

echo "Jetson setup complete."
echo "Log out and back in so dialout/plugdev group membership takes effect."
echo "Next: copy config/hwci.example.yaml to config/hwci.yaml and edit device commands."
echo "Jetson IP addresses:"
hostname -I || true


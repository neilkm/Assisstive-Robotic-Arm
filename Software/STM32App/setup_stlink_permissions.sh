#!/usr/bin/env bash
set -euo pipefail

UDEV_RULES_URL="https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules"
UDEV_RULES_PATH="/etc/udev/rules.d/99-platformio-udev.rules"
TARGET_USER="${SUDO_USER:-${USER}}"

if [[ "${EUID}" -eq 0 ]]; then
    echo "Run this script as a normal user. It will use sudo for system changes."
    exit 1
fi

if ! command -v sudo >/dev/null 2>&1; then
    echo "sudo is required to install udev rules and update user groups."
    exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "curl is required to download PlatformIO udev rules."
    echo "Install it with: sudo apt install curl"
    exit 1
fi

echo "Adding ${TARGET_USER} to plugdev and dialout..."
sudo usermod -aG plugdev,dialout "${TARGET_USER}"

echo "Installing PlatformIO udev rules to ${UDEV_RULES_PATH}..."
curl -fsSL "${UDEV_RULES_URL}" | sudo tee "${UDEV_RULES_PATH}" >/dev/null

echo "Reloading udev rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger

echo
echo "Done. Unplug and replug the Nucleo board."
echo "Log out and back in, or reboot the Jetson, so the group changes apply."
echo
echo "After reconnecting, verify with:"
echo "  lsusb | grep -i -E 'st|0483'"
echo "  groups"
echo
echo "Then upload with:"
echo "  pio run -t upload"

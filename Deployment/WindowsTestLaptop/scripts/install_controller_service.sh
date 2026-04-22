#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
SERVICE_TEMPLATE="$REPO_ROOT/Deployment/WindowsTestLaptop/systemd/hwci-controller.service"
SERVICE_DIR="$HOME/.config/systemd/user"
SERVICE_FILE="$SERVICE_DIR/hwci-controller.service"
PYTHON_BIN="${PYTHON_BIN:-$REPO_ROOT/.venv/bin/python}"
CONFIG="${CONFIG:-$REPO_ROOT/Deployment/WindowsTestLaptop/config/hwci.yaml}"

if [ ! -f "$CONFIG" ]; then
  echo "Missing local config: $CONFIG"
  echo "Create it from Deployment/WindowsTestLaptop/config/hwci.example.yaml first."
  exit 1
fi

mkdir -p "$SERVICE_DIR"
sed \
  -e "s#__REPO_ROOT__#$REPO_ROOT#g" \
  -e "s#__PYTHON_BIN__#$PYTHON_BIN#g" \
  -e "s#__CONFIG__#$CONFIG#g" \
  "$SERVICE_TEMPLATE" > "$SERVICE_FILE"

systemctl --user daemon-reload
systemctl --user enable --now hwci-controller.service

if command -v loginctl >/dev/null 2>&1; then
  loginctl enable-linger "$USER" >/dev/null 2>&1 || true
fi

echo "Installed and started user service: hwci-controller"
echo "Check status with: systemctl --user status hwci-controller"


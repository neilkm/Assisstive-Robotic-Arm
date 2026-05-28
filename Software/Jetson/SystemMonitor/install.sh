#!/usr/bin/env bash
# Install System-Monitor-NNK to /usr/local/bin (or INSTALL_DIR).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_SOURCE="${SCRIPT_DIR}/System-Monitor-NNK"
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
INSTALL_PATH="${INSTALL_DIR}/System-Monitor-NNK"

python3 -c "import curses" 2>/dev/null || { echo "error: python3 curses required"; exit 1; }
[[ -f "${APP_SOURCE}" ]] || { echo "error: missing ${APP_SOURCE}"; exit 1; }

if [[ ! -d "${INSTALL_DIR}" ]]; then
    sudo mkdir -p "${INSTALL_DIR}"
fi

if [[ -w "${INSTALL_DIR}" ]]; then
    install -m 0755 "${APP_SOURCE}" "${INSTALL_PATH}"
else
    sudo install -m 0755 "${APP_SOURCE}" "${INSTALL_PATH}"
fi

echo "Installed to ${INSTALL_PATH}"
echo "Run: System-Monitor-NNK"

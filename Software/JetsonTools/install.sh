#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_NAME="System-Monitor-NNK"
APP_SOURCE="${SCRIPT_DIR}/${APP_NAME}"
SSH_HELPER_NAME="ssh_jetson.sh"
SSH_HELPER_SOURCE="${SCRIPT_DIR}/${SSH_HELPER_NAME}"
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
INSTALL_PATH="${INSTALL_DIR}/${APP_NAME}"
SSH_HELPER_INSTALL_PATH="${INSTALL_DIR}/${SSH_HELPER_NAME}"

echo "== JetsonTools ${APP_NAME} installer =="

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required to run ${APP_NAME}."
  exit 1
fi

python3 - <<'PY'
import curses  # noqa: F401
PY

if [[ ! -f "${APP_SOURCE}" ]]; then
  echo "Missing app source: ${APP_SOURCE}"
  exit 1
fi

if [[ ! -f "${SSH_HELPER_SOURCE}" ]]; then
  echo "Missing SSH helper source: ${SSH_HELPER_SOURCE}"
  exit 1
fi

if [[ ! -d "${INSTALL_DIR}" ]]; then
  echo "Creating install directory: ${INSTALL_DIR}"
  if [[ -w "$(dirname "${INSTALL_DIR}")" ]]; then
    mkdir -p "${INSTALL_DIR}"
  else
    sudo mkdir -p "${INSTALL_DIR}"
  fi
fi

if [[ -w "${INSTALL_DIR}" ]]; then
  install -m 0755 "${APP_SOURCE}" "${INSTALL_PATH}"
  install -m 0755 "${SSH_HELPER_SOURCE}" "${SSH_HELPER_INSTALL_PATH}"
else
  sudo install -m 0755 "${APP_SOURCE}" "${INSTALL_PATH}"
  sudo install -m 0755 "${SSH_HELPER_SOURCE}" "${SSH_HELPER_INSTALL_PATH}"
fi

echo "Installed ${APP_NAME} to ${INSTALL_PATH}"
echo "Installed ${SSH_HELPER_NAME} to ${SSH_HELPER_INSTALL_PATH}"
echo "Run with: ${APP_NAME}"
echo "SSH to Jetson with: ${SSH_HELPER_NAME}"

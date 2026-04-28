#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-$REPO_ROOT/.venv/bin/python}"
CONFIG="${CONFIG:-$REPO_ROOT/Deployment/JetsonController/config/hwci.yaml}"

exec "$PYTHON_BIN" "$REPO_ROOT/Deployment/JetsonController/app/hwci_controller.py" \
  --config "$CONFIG" \
  --poll \
  "$@"

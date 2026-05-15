#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

MODE="${1:-gui}"

case "$MODE" in
  gui)
    shift || true
    "${SCRIPT_DIR}/run_qml_app.sh" "$@"
    ;;
  smoke)
    "${SCRIPT_DIR}/run_backend_app.sh" --smoke
    ;;
  backend)
    shift || true
    "${SCRIPT_DIR}/run_backend_app.sh" "$@"
    ;;
  qml)
    shift || true
    "${SCRIPT_DIR}/run_qml_app.sh" "$@"
    ;;
  *)
    echo "Usage: ./run.sh [gui|qml|backend|smoke] [app args...]" >&2
    exit 2
    ;;
esac

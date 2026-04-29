#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
APP="${PROJECT_DIR}/build/apps/StateMachine_UI/StateMachine_UI"

if [[ ! -x "${APP}" ]]; then
    "${SCRIPT_DIR}/build_all_apps.sh"
fi

exec "${APP}"

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
APP="${PROJECT_DIR}/build/apps/StateMachine_UI/StateMachine_UI"
MACOS_APP="${PROJECT_DIR}/build/apps/StateMachine_UI/StateMachine_UI.app/Contents/MacOS/StateMachine_UI"

if [[ -x "${MACOS_APP}" ]]; then
    exec "${MACOS_APP}"
fi

if [[ -x "${APP}" ]]; then
    exec "${APP}"
fi

"${SCRIPT_DIR}/build_all_apps.sh"

if [[ -x "${MACOS_APP}" ]]; then
    exec "${MACOS_APP}"
fi

exec "${APP}"

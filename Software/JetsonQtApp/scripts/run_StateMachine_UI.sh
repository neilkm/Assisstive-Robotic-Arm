#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
APP="${PROJECT_DIR}/build/apps/StateMachine_UI/StateMachine_UI"
MACOS_APP="${PROJECT_DIR}/build/apps/StateMachine_UI/StateMachine_UI.app/Contents/MacOS/StateMachine_UI"

if [[ -x "${MACOS_APP}" ]]; then
    exec "${MACOS_APP}"
fi

configure_local_jetson_display() {
    if [[ "$(uname -s)" != "Linux" ]]; then
        return
    fi

    # The app is operated from the Jetson's attached display. SSH sessions often
    # have DISPLAY unset or pointed at SSH X11 forwarding, so default to the
    # local X server unless the caller explicitly overrides it.
    export DISPLAY="${JETSON_QT_DISPLAY:-:0}"
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"

    local runtime_dir="/run/user/$(id -u)"
    if [[ -z "${XDG_RUNTIME_DIR:-}" && -d "${runtime_dir}" ]]; then
        export XDG_RUNTIME_DIR="${runtime_dir}"
    fi

    if [[ -z "${XAUTHORITY:-}" ]]; then
        local candidate
        for candidate in \
            "${HOME}/.Xauthority" \
            "${XDG_RUNTIME_DIR:-}/gdm/Xauthority" \
            "${XDG_RUNTIME_DIR:-}/Xauthority"; do
            if [[ -f "${candidate}" ]]; then
                export XAUTHORITY="${candidate}"
                break
            fi
        done
    fi
}

configure_local_jetson_display

if [[ -x "${APP}" ]]; then
    exec "${APP}"
fi

"${SCRIPT_DIR}/build_all_apps.sh"

if [[ -x "${MACOS_APP}" ]]; then
    exec "${MACOS_APP}"
fi

exec "${APP}"

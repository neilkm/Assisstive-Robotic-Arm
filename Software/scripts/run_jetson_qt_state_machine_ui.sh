#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/../JetsonQtApp" && pwd)"
APP="${PROJECT_DIR}/build/apps/StateMachine_UI/StateMachine_UI"
MACOS_APP="${PROJECT_DIR}/build/apps/StateMachine_UI/StateMachine_UI.app/Contents/MacOS/StateMachine_UI"

if [[ -x "${MACOS_APP}" ]]; then
    exec "${MACOS_APP}"
fi

configure_local_jetson_display() {
    if [[ "$(uname -s)" != "Linux" ]]; then
        return
    fi

    local runtime_dir="/run/user/$(id -u)"
    if [[ -z "${XDG_RUNTIME_DIR:-}" && -d "${runtime_dir}" ]]; then
        export XDG_RUNTIME_DIR="${runtime_dir}"
    fi

    # The app is operated from the Jetson's attached display. SSH sessions often
    # have DISPLAY unset or pointed at SSH X11 forwarding, so detect the local
    # desktop socket instead of inheriting the SSH session display.
    if [[ -n "${JETSON_QT_DISPLAY:-}" ]]; then
        export DISPLAY="${JETSON_QT_DISPLAY}"
        export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
    elif [[ -z "${QT_QPA_PLATFORM:-}" ]]; then
        local wayland_socket
        for wayland_socket in "${XDG_RUNTIME_DIR:-}"/wayland-*; do
            if [[ -S "${wayland_socket}" ]]; then
                export WAYLAND_DISPLAY="$(basename "${wayland_socket}")"
                export QT_QPA_PLATFORM="wayland"
                break
            fi
        done

        if [[ -z "${QT_QPA_PLATFORM:-}" ]]; then
            local x_socket
            for x_socket in /tmp/.X11-unix/X*; do
                if [[ -S "${x_socket}" ]]; then
                    export DISPLAY=":${x_socket##*X}"
                    export QT_QPA_PLATFORM="xcb"
                    break
                fi
            done
        fi

        if [[ -z "${QT_QPA_PLATFORM:-}" ]]; then
            export DISPLAY=":0"
            export QT_QPA_PLATFORM="xcb"
        fi
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

"${SCRIPT_DIR}/build_jetson_qt_apps.sh"

if [[ -x "${MACOS_APP}" ]]; then
    exec "${MACOS_APP}"
fi

exec "${APP}"

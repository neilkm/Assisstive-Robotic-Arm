#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCENE_PATH="${SCRIPT_DIR}/scene/arm_scene.ttt"
FALLBACK_SCENE="${SCRIPT_DIR}/SceneV1.ttt"

if [[ ! -f "${SCENE_PATH}" ]]; then
    SCENE_PATH="${FALLBACK_SCENE}"
fi

if [[ ! -f "${SCENE_PATH}" ]]; then
    echo "No CoppeliaSim scene found." >&2
    echo "Expected ${SCRIPT_DIR}/scene/arm_scene.ttt or ${FALLBACK_SCENE}" >&2
    exit 1
fi

launch_binary() {
    local app_path="$1"
    local binary_path="${app_path}/Contents/MacOS/coppeliaSim"

    if [[ -x "${binary_path}" ]]; then
        exec "${binary_path}" "${SCENE_PATH}"
    fi

    return 1
}

if [[ -n "${COPPELIASIM_APP:-}" ]]; then
    if launch_binary "${COPPELIASIM_APP}"; then
        exit 0
    fi
fi

if [[ -d "${HOME}/Applications/coppeliaSim.app" ]]; then
    if launch_binary "${HOME}/Applications/coppeliaSim.app"; then
        exit 0
    fi
fi

if [[ -d "/Applications/CoppeliaSim.app" ]]; then
    if launch_binary "/Applications/CoppeliaSim.app"; then
        exit 0
    fi
fi

if [[ -d "/Applications/coppeliaSim.app" ]]; then
    if launch_binary "/Applications/coppeliaSim.app"; then
        exit 0
    fi
fi

echo "Set COPPELIASIM_APP to your CoppeliaSim.app path." >&2
echo "Example: COPPELIASIM_APP=${HOME}/Applications/coppeliaSim.app ./run_coppelia.sh" >&2
echo "Expected executable at <app>/Contents/MacOS/coppeliaSim" >&2
exit 1

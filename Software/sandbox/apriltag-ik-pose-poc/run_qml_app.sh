#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

"${SCRIPT_DIR}/build.sh"

if [[ "${1:-}" == "--smoke" ]]; then
  QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
  APRILTAG_IK_POSE_POC_ROOT="${SCRIPT_DIR}" \
    "${BUILD_DIR}/apriltag_ik_pose_qml" --smoke
else
  APRILTAG_IK_POSE_POC_ROOT="${SCRIPT_DIR}" \
    "${BUILD_DIR}/apriltag_ik_pose_qml" "$@"
fi

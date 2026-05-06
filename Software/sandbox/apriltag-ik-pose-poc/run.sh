#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

MODE="${1:-gui}"

"$SCRIPT_DIR/build.sh"

case "$MODE" in
  gui)
    APRILTAG_IK_POSE_POC_ROOT="$SCRIPT_DIR" build/apriltag_ik_pose_qml
    ;;
  smoke)
    APRILTAG_IK_POSE_POC_ROOT="$SCRIPT_DIR" build/apriltag_ik_pose_backend --smoke
    ;;
  *)
    echo "Usage: ./run.sh [gui|smoke]" >&2
    exit 2
    ;;
esac

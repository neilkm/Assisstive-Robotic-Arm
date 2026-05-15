#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Removing build and test output under ${SCRIPT_DIR}"
rm -rf "${SCRIPT_DIR}/build"
rm -rf "${SCRIPT_DIR}/.pytest_cache"
rm -rf "${SCRIPT_DIR}/.venv"
find "${SCRIPT_DIR}" -type d -name "__pycache__" -prune -exec rm -rf {} +
find "${SCRIPT_DIR}" -type f \( -name "*.pyc" -o -name "*.pyo" \) -delete
rm -f /tmp/apriltag_ik_pose_trajectory.json
echo "Clean complete."

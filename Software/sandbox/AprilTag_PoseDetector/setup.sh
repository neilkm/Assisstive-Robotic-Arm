#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="${REPO_ROOT}/.venv"
PYTHON_BIN="${PYTHON_BIN:-python3}"

CALIBRATE=0
if [[ "${1:-}" == "--calibrate" ]]; then
  CALIBRATE=1
elif [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  echo "Usage:"
  echo "  ./setup.sh            # install + run detector"
  echo "  ./setup.sh --calibrate # calibrate then run detector"
  exit 0
elif [[ $# -gt 0 ]]; then
  echo "Unknown argument: $1"
  echo "Run: ./setup.sh --help"
  exit 2
fi

if ! command -v "${PYTHON_BIN}" >/dev/null 2>&1; then
  echo "ERROR: ${PYTHON_BIN} not found. Install Python 3."
  exit 1
fi

if [[ ! -f "${REPO_ROOT}/requirements.txt" ]]; then
  echo "ERROR: requirements.txt not found."
  exit 1
fi

echo "== AprilTag_PoseDetector =="
echo "Repo: ${REPO_ROOT}"

if [[ ! -d "${VENV_DIR}" ]]; then
  echo "== Creating venv: ${VENV_DIR}"
  "${PYTHON_BIN}" -m venv "${VENV_DIR}"
else
  echo "== Using existing venv: ${VENV_DIR}"
fi

# shellcheck disable=SC1091
source "${VENV_DIR}/bin/activate"

echo "== Upgrading pip"
python -m pip install --upgrade pip >/dev/null

echo "== Installing dependencies"
python -m pip install -r "${REPO_ROOT}/requirements.txt" >/dev/null

echo "== Verifying OpenCV aruco support"
python - <<'PY'
import cv2
assert hasattr(cv2, "aruco"), "cv2.aruco missing. Ensure opencv-contrib-python is installed."
print("OpenCV:", cv2.__version__)
print("cv2.aruco: OK")
PY

if [[ "$CALIBRATE" -eq 1 ]]; then
  echo "== Running calibration (src/camera.yaml will be created)"
  python "${REPO_ROOT}/src/camera_calibrate.py"
fi

echo "== Running detector"
python "${REPO_ROOT}/src/detect_pose.py"


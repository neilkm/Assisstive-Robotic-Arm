#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
PYTHON_BIN="${PYTHON:-python3}"
QMLTESTRUNNER="${QMLTESTRUNNER:-qmltestrunner}"

section() {
  printf '\n== %s ==\n' "$1"
}

section "Build"
"${SCRIPT_DIR}/build.sh"

section "C++ unit tests"
APRILTAG_IK_POSE_POC_ROOT="${SCRIPT_DIR}" "${BUILD_DIR}/apriltag_ik_pose_unit_tests"
APRILTAG_IK_POSE_POC_ROOT="${SCRIPT_DIR}" "${BUILD_DIR}/arm_forward_kinematics_tests"
APRILTAG_IK_POSE_POC_ROOT="${SCRIPT_DIR}" "${BUILD_DIR}/april_tag_end_effector_estimator_tests"

section "QML UI tests"
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
"${QMLTESTRUNNER}" \
  -input "${SCRIPT_DIR}/tests/qml" \
  -import "${SCRIPT_DIR}/qml"

section "Backend CLI smoke"
APRILTAG_IK_POSE_POC_ROOT="${SCRIPT_DIR}" "${BUILD_DIR}/apriltag_ik_pose_backend" --smoke

section "Run script smoke"
"${SCRIPT_DIR}/run_backend_app.sh" --smoke
"${SCRIPT_DIR}/run_qml_app.sh" --smoke

section "Trajectory CLI smoke"
APRILTAG_IK_POSE_POC_ROOT="${SCRIPT_DIR}" "${BUILD_DIR}/apriltag_ik_pose_backend" \
  --trajectory 0 0 0 0 0 0 5 0 0 0 0 0 5 5 >/tmp/apriltag_ik_pose_trajectory.json
"${PYTHON_BIN}" -m json.tool /tmp/apriltag_ik_pose_trajectory.json >/dev/null
echo "Trajectory JSON is valid."

section "All tests passed"

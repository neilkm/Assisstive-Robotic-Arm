#!/usr/bin/env bash
set -euo pipefail

JETSON_SSH_HOST="${JETSON_SSH_HOST:-jetson-arm.tail7a7a89.ts.net}"
JETSON_SSH_USER="${JETSON_SSH_USER:-jetson}"

exec ssh "${JETSON_SSH_USER}@${JETSON_SSH_HOST}" "$@"

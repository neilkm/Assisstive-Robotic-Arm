#!/usr/bin/env bash
set -euo pipefail

DEVICE_NAME="${DEVICE_NAME:-ArmESP32Buttons}"
RFCOMM_DEVICE="${RFCOMM_DEVICE:-/dev/rfcomm0}"
RFCOMM_CHANNEL="${RFCOMM_CHANNEL:-1}"

if ! command -v bluetoothctl >/dev/null 2>&1; then
    echo "error: bluetoothctl is required" >&2
    exit 1
fi

echo "Powering Bluetooth and scanning for ${DEVICE_NAME}..."
bluetoothctl power on >/dev/null
bluetoothctl agent KeyboardOnly >/dev/null
bluetoothctl default-agent >/dev/null

bluetoothctl scan on >/dev/null &
SCAN_PID=$!
sleep 10
kill "${SCAN_PID}" >/dev/null 2>&1 || true
bluetoothctl scan off >/dev/null 2>&1 || true

DEVICE_MAC="$(bluetoothctl devices | awk -v name="${DEVICE_NAME}" '$0 ~ name { print $2; exit }')"
if [[ -z "${DEVICE_MAC}" ]]; then
    echo "error: could not find ${DEVICE_NAME}; make sure the ESP32 is powered and advertising" >&2
    exit 1
fi

echo "Pairing and trusting ${DEVICE_NAME} at ${DEVICE_MAC}..."
bluetoothctl <<EOF
pair ${DEVICE_MAC}
1234
trust ${DEVICE_MAC}
connect ${DEVICE_MAC}
EOF

echo "Binding ${RFCOMM_DEVICE} to ${DEVICE_MAC} channel ${RFCOMM_CHANNEL}..."
if [[ -e "${RFCOMM_DEVICE}" ]]; then
    sudo rfcomm release "${RFCOMM_DEVICE}" >/dev/null 2>&1 || true
fi
sudo rfcomm bind "${RFCOMM_DEVICE}" "${DEVICE_MAC}" "${RFCOMM_CHANNEL}"

echo "${RFCOMM_DEVICE} is bound to ${DEVICE_NAME}. Use this path with jetson_esp32_bluetooth_protocol."

#!/usr/bin/env bash
set -euo pipefail

DEVICE_NAME="${DEVICE_NAME:-ArmESP32Buttons}"
RFCOMM_DEVICE="${RFCOMM_DEVICE:-/dev/rfcomm0}"
RFCOMM_CHANNEL="${RFCOMM_CHANNEL:-1}"
PAIR_PIN="${PAIR_PIN:-1234}"
SCAN_SECONDS="${SCAN_SECONDS:-15}"

if ! command -v bluetoothctl >/dev/null 2>&1; then
    echo "error: bluetoothctl is required" >&2
    exit 1
fi
if ! command -v rfcomm >/dev/null 2>&1; then
    echo "error: rfcomm is required. Install bluez utilities on the Jetson." >&2
    exit 1
fi

echo "Powering Bluetooth and scanning for ${DEVICE_NAME}..."
if command -v rfkill >/dev/null 2>&1; then
    sudo rfkill unblock bluetooth || true
fi
if command -v systemctl >/dev/null 2>&1; then
    sudo systemctl start bluetooth || true
fi

if ! bluetoothctl show >/dev/null 2>&1; then
    echo "error: no Bluetooth controller is visible to bluetoothctl" >&2
    echo "       Check the Jetson Bluetooth adapter, rfkill state, and bluetooth service." >&2
    exit 1
fi

bluetoothctl power on
bluetoothctl agent KeyboardOnly || bluetoothctl agent NoInputNoOutput
bluetoothctl default-agent

bluetoothctl scan on >/dev/null &
SCAN_PID=$!
sleep "${SCAN_SECONDS}"
kill "${SCAN_PID}" >/dev/null 2>&1 || true
bluetoothctl scan off >/dev/null 2>&1 || true

DEVICE_MAC="$(bluetoothctl devices | awk -v name="${DEVICE_NAME}" '$0 ~ name { print $2; exit }')"
if [[ -z "${DEVICE_MAC}" ]]; then
    echo "error: could not find ${DEVICE_NAME}; make sure the ESP32 is powered and advertising" >&2
    echo "       Nearby devices seen by bluetoothctl:" >&2
    bluetoothctl devices >&2 || true
    exit 1
fi

echo "Pairing and trusting ${DEVICE_NAME} at ${DEVICE_MAC}..."
if command -v expect >/dev/null 2>&1; then
    expect -c '
        set timeout 30
        set mac [lindex $argv 0]
        set pin [lindex $argv 1]
        spawn bluetoothctl
        expect {
            -re "# " {}
            timeout { exit 2 }
        }
        send "agent KeyboardOnly\r"
        expect {
            -re "# " {}
            timeout { exit 3 }
        }
        send "default-agent\r"
        expect {
            -re "# " {}
            timeout { exit 4 }
        }
        send "pair $mac\r"
        expect {
            -re "PIN code:|Enter PIN code" {
                send "$pin\r"
                exp_continue
            }
            -re "Pairing successful|AlreadyExists|already paired|Connection successful" {}
            -re "Failed to pair" { exit 5 }
            timeout { exit 6 }
        }
        send "trust $mac\r"
        expect {
            -re "trust succeeded|Changing .* trust succeeded|# " {}
            timeout { exit 7 }
        }
        send "connect $mac\r"
        expect {
            -re "Connection successful|Failed to connect|# " {}
            timeout { exit 8 }
        }
        send "quit\r"
    ' "${DEVICE_MAC}" "${PAIR_PIN}"
else
    echo "warning: expect not found; attempting non-interactive bluetoothctl pairing" >&2
    bluetoothctl <<EOF
pair ${DEVICE_MAC}
${PAIR_PIN}
trust ${DEVICE_MAC}
connect ${DEVICE_MAC}
EOF
fi

echo "Binding ${RFCOMM_DEVICE} to ${DEVICE_MAC} channel ${RFCOMM_CHANNEL}..."
if [[ -e "${RFCOMM_DEVICE}" ]]; then
    sudo rfcomm release "${RFCOMM_DEVICE}" >/dev/null 2>&1 || true
fi
sudo rfcomm bind "${RFCOMM_DEVICE}" "${DEVICE_MAC}" "${RFCOMM_CHANNEL}"

echo "${RFCOMM_DEVICE} is bound to ${DEVICE_NAME}. Use this path with jetson_esp32_bluetooth_protocol."

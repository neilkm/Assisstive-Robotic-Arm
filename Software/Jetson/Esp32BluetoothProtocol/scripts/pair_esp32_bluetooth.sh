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

bluetooth_info_has() {
    local mac="$1"
    local key="$2"
    local value="$3"
    bluetoothctl info "${mac}" 2>/dev/null | awk -v key="${key}" -v value="${value}" '
        $1 == key ":" && $2 == value { found = 1 }
        END { exit found ? 0 : 1 }
    '
}

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
if bluetooth_info_has "${DEVICE_MAC}" "Paired" "yes"; then
    echo "${DEVICE_NAME} is already paired."
elif command -v expect >/dev/null 2>&1; then
    EXPECT_DEVICE_MAC="${DEVICE_MAC}" EXPECT_PAIR_PIN="${PAIR_PIN}" expect -c '
        set timeout 30
        set mac $env(EXPECT_DEVICE_MAC)
        set pin $env(EXPECT_PAIR_PIN)
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
    '
else
    echo "error: ${DEVICE_NAME} is not paired and expect is not installed." >&2
    echo "       Install it on the Jetson with: sudo apt-get install expect" >&2
    echo "       Then rerun this test so the helper can answer the Bluetooth PIN ${PAIR_PIN} prompt." >&2
    exit 1
fi

bluetoothctl trust "${DEVICE_MAC}"
bluetoothctl connect "${DEVICE_MAC}" || true

if ! bluetooth_info_has "${DEVICE_MAC}" "Paired" "yes"; then
    echo "error: ${DEVICE_NAME} is still not paired after pairing attempt." >&2
    bluetoothctl info "${DEVICE_MAC}" >&2 || true
    exit 1
fi
if ! bluetooth_info_has "${DEVICE_MAC}" "Trusted" "yes"; then
    echo "error: ${DEVICE_NAME} is still not trusted after trust attempt." >&2
    bluetoothctl info "${DEVICE_MAC}" >&2 || true
    exit 1
fi

echo "Pair/trust status verified for ${DEVICE_MAC}."

echo "Binding ${RFCOMM_DEVICE} to ${DEVICE_MAC} channel ${RFCOMM_CHANNEL}..."
if [[ -e "${RFCOMM_DEVICE}" ]]; then
    sudo rfcomm release "${RFCOMM_DEVICE}" >/dev/null 2>&1 || true
fi
sudo rfcomm bind "${RFCOMM_DEVICE}" "${DEVICE_MAC}" "${RFCOMM_CHANNEL}"

echo "${RFCOMM_DEVICE} is bound to ${DEVICE_NAME}. Use this path with jetson_esp32_bluetooth_protocol."

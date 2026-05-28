#!/usr/bin/env python3

import argparse
import re
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
JETSON_PROTOCOL_DIR = REPO_ROOT / "Software" / "Jetson" / "Esp32BluetoothProtocol"
BUILD_DIR = REPO_ROOT / "builds" / "JetsonEsp32BluetoothProtocol"
ESP32_DIR = REPO_ROOT / "Software" / "ESP32"
STATE_RE = re.compile(r"rx_state seq=(\d+) mask=0x([0-9a-fA-F]{2}) buttons=\[([^\]]*)\]")
ACK_RE = re.compile(r"tx_ack seq=(\d+)")
SUMMARY_RE = re.compile(r"^(rx_bytes|valid_frames|state_frames|ack_tx_frames|non_state_frames)=(\d+)$")


def print_section(title):
    print()
    print("=" * 72)
    print(title)
    print("=" * 72)


def run(command, cwd=None, capture=False):
    print(f"+ {' '.join(str(part) for part in command)}")
    if capture:
        return subprocess.run(command, cwd=cwd, check=True, text=True, capture_output=True)
    subprocess.run(command, cwd=cwd, check=True)
    return None


def print_listener_summary(output):
    states = []
    ack_sequences = []
    counters = {}

    for line in output.splitlines():
        state_match = STATE_RE.match(line)
        if state_match:
            states.append(
                {
                    "sequence": int(state_match.group(1)),
                    "mask": int(state_match.group(2), 16),
                    "buttons": state_match.group(3),
                }
            )
            continue

        ack_match = ACK_RE.match(line)
        if ack_match:
            ack_sequences.append(int(ack_match.group(1)))
            continue

        summary_match = SUMMARY_RE.match(line)
        if summary_match:
            counters[summary_match.group(1)] = int(summary_match.group(2))

    print_section("Bluetooth Button Test Summary")
    print(f"Received state frames: {len(states)}")
    print(f"Transmitted ACKs:      {len(ack_sequences)}")
    if counters:
        print(f"Raw RX bytes:          {counters.get('rx_bytes', 0)}")
        print(f"Valid frames:          {counters.get('valid_frames', 0)}")
        print(f"Non-state frames:      {counters.get('non_state_frames', 0)}")

    if states:
        print()
        print("Last random button samples:")
        for state in states[-10:]:
            print(f"  seq={state['sequence']:3d} mask=0x{state['mask']:02X} buttons=[{state['buttons']}]")

    missing_acks = [
        state["sequence"]
        for state in states
        if state["sequence"] not in ack_sequences
    ]
    if missing_acks:
        print()
        print(f"Missing ACKs for received sequences: {missing_acks}")
    elif states:
        print()
        print("Every received state frame was ACKed.")


def main():
    parser = argparse.ArgumentParser(
        description="Build the Jetson ESP32 Bluetooth protocol app, optionally flash ESP32 firmware, and exercise button telemetry."
    )
    parser.add_argument("--device", default="/dev/rfcomm0", help="Jetson RFCOMM device bound to the ESP32 SPP service")
    parser.add_argument("--baud", default="115200", help="TTY baudrate applied to the RFCOMM device")
    parser.add_argument("--duration-ms", default="10000", help="How long the Jetson listener should run")
    parser.add_argument("--skip-flash", action="store_true", help="Do not flash the ESP32 before running")
    parser.add_argument("--skip-pair", action="store_true", help="Do not run the Jetson bluetoothctl/rfcomm pairing helper")
    parser.add_argument("--upload-port", default=None, help="Optional PlatformIO ESP32 upload port")
    parser.add_argument("--rx-hex", action="store_true", help="Print raw RX bytes seen by the Jetson app")
    args = parser.parse_args()

    print_section("Build Jetson Bluetooth Listener")
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    run(["cmake", "-S", str(JETSON_PROTOCOL_DIR), "-B", str(BUILD_DIR)])
    run(["cmake", "--build", str(BUILD_DIR)])
    run(["ctest", "--test-dir", str(BUILD_DIR), "--output-on-failure"])

    if not args.skip_flash:
        print_section("Flash ESP32 Random Button Test Firmware")
        flash_command = ["pio", "run", "-e", "esp32dev", "-t", "upload"]
        if args.upload_port:
            flash_command.extend(["--upload-port", args.upload_port])
        run(flash_command, cwd=ESP32_DIR)
    else:
        print_section("Skip ESP32 Flash")
        print("Using the firmware already on the ESP32.")

    if not args.skip_pair:
        print_section("Pair And Bind ESP32 Bluetooth")
        run([str(JETSON_PROTOCOL_DIR / "scripts" / "pair_esp32_bluetooth.sh")])
    else:
        print_section("Skip Bluetooth Pair/Bind")
        print(f"Using existing RFCOMM device at {args.device}.")

    print_section("Run Jetson Listener")
    app = BUILD_DIR / "jetson_esp32_bluetooth_protocol"
    app_command = [
        str(app),
        "--device",
        args.device,
        "--baud",
        args.baud,
        "--duration-ms",
        args.duration_ms,
    ]
    if args.rx_hex:
        app_command.append("--rx-hex")
    result = run(app_command, capture=True)
    print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="")
    print_listener_summary(result.stdout)


if __name__ == "__main__":
    main()

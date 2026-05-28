#!/usr/bin/env python3

import argparse
import os
import select
import subprocess
import sys
import termios
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
JETSON_PROTOCOL_DIR = REPO_ROOT / "Software" / "Jetson" / "Esp32BluetoothProtocol"
ESP32_DIR = REPO_ROOT / "Software" / "ESP32"


def print_section(title):
    print()
    print("=" * 72)
    print(title)
    print("=" * 72)


def run(command, cwd=None):
    print(f"+ {' '.join(str(part) for part in command)}")
    try:
        subprocess.run(command, cwd=cwd, check=True)
    except subprocess.CalledProcessError as exc:
        print(f"Command failed with exit code {exc.returncode}: {' '.join(str(part) for part in command)}")
        sys.exit(exc.returncode)


def configure_serial(fd, baudrate):
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CLOCAL | termios.CREAD | termios.CS8
    attrs[3] = 0
    attrs[4] = baudrate
    attrs[5] = baudrate
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)


def baudrate_constant(baud):
    mapping = {
        9600: termios.B9600,
        19200: termios.B19200,
        38400: termios.B38400,
        57600: termios.B57600,
        115200: termios.B115200,
        230400: termios.B230400,
    }
    if baud not in mapping:
        raise ValueError(f"unsupported baudrate: {baud}")
    return mapping[baud]


def read_until(fd, expected, timeout_s):
    deadline = time.monotonic() + timeout_s
    chunks = []
    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], 0.1)
        if not ready:
            continue
        data = os.read(fd, 256)
        if not data:
            continue
        text = data.decode("utf-8", errors="replace")
        print(text, end="", flush=True)
        chunks.append(text)
        combined = "".join(chunks)
        if expected in combined:
            return combined
    return "".join(chunks)


def main():
    parser = argparse.ArgumentParser(
        description="Flash ESP32 Bluetooth UART echo firmware, pair over Bluetooth, send a Jetson terminal string, and verify the echo."
    )
    parser.add_argument("--device", default="/dev/rfcomm0", help="Jetson RFCOMM device bound to the ESP32 SPP service")
    parser.add_argument("--baud", type=int, default=115200, help="TTY baudrate applied to the RFCOMM device")
    parser.add_argument("--skip-flash", action="store_true", help="Do not flash the ESP32 echo firmware before running")
    parser.add_argument("--skip-pair", action="store_true", help="Do not run the Jetson bluetoothctl/rfcomm pairing helper")
    parser.add_argument("--upload-port", default=None, help="Optional PlatformIO ESP32 upload port")
    parser.add_argument("--timeout-s", type=float, default=15.0, help="Timeout for banner and echo reads")
    args = parser.parse_args()

    if not args.skip_flash:
        print_section("Flash ESP32 Bluetooth UART Echo Test Firmware")
        flash_command = ["pio", "run", "-e", "esp32dev_bluetooth_echo_test", "-t", "upload"]
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

    print_section("Open Bluetooth UART")
    fd = os.open(args.device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        configure_serial(fd, baudrate_constant(args.baud))
        print(f"Connected to {args.device}. Waiting for ESP32 test runner banner...")
        banner = read_until(fd, "ESP32 Bluetooth UART Echo Test Runner", args.timeout_s)
        if "ESP32 Bluetooth UART Echo Test Runner" not in banner:
            print()
            print("Did not receive the ESP32 echo test banner.")
            sys.exit(1)

        print()
        message = input("Enter string to send to ESP32, then press Enter: ")
        expected = f"Rx [{message}]"
        os.write(fd, (message + "\n").encode("utf-8"))

        print("Waiting for echo...")
        response = read_until(fd, expected, args.timeout_s)
        if expected not in response:
            print()
            print(f"Expected echo was not observed: {expected}")
            sys.exit(1)

        print()
        print("Bluetooth UART echo test passed.")
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()

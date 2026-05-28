#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
ESP32_DIR = REPO_ROOT / "Software" / "ESP32"


def run(command, cwd=None):
    print(f"+ {' '.join(str(part) for part in command)}")
    subprocess.run(command, cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser(
        description="Build and flash the current ESP32 Bluetooth random button telemetry test."
    )
    parser.add_argument("--upload-port", default=None, help="Optional PlatformIO ESP32 upload port")
    args = parser.parse_args()

    command = ["pio", "run", "-e", "esp32dev", "-t", "upload"]
    if args.upload_port:
        command.extend(["--upload-port", args.upload_port])
    run(command, cwd=ESP32_DIR)


if __name__ == "__main__":
    main()

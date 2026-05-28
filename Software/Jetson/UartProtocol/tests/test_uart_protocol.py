#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
JETSON_PROTOCOL_DIR = REPO_ROOT / "Software" / "Jetson" / "UartProtocol"
BUILD_DIR = REPO_ROOT / "builds" / "JetsonUartProtocol"
STM32_DIR = REPO_ROOT / "Software" / "STM32"


def run(command, cwd=None):
    print(f"+ {' '.join(str(part) for part in command)}")
    subprocess.run(command, cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser(
        description="Build the Jetson UART protocol app, flash the STM protocol harness, and exercise packet transfer."
    )
    parser.add_argument("--device", default="/dev/ttyTHS1", help="Jetson UART device connected to STM USART2")
    parser.add_argument("--baud", default="115200", help="UART baudrate")
    parser.add_argument("--iterations", default="20", help="Jetson desired-state packets to send")
    parser.add_argument("--period-ms", default="100", help="Delay between Jetson desired-state packets")
    parser.add_argument("--upload-port", default=None, help="Optional PlatformIO STM32 upload port")
    parser.add_argument("--skip-flash", action="store_true", help="Do not flash the STM32 before running")
    args = parser.parse_args()

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    run(["cmake", "-S", str(JETSON_PROTOCOL_DIR), "-B", str(BUILD_DIR)])
    run(["cmake", "--build", str(BUILD_DIR)])

    if not args.skip_flash:
        flash_command = ["pio", "run", "-e", "nucleo_f446re_uart_protocol", "-t", "upload"]
        if args.upload_port:
            flash_command.extend(["--upload-port", args.upload_port])
        run(flash_command, cwd=STM32_DIR)

    app = BUILD_DIR / "jetson_uart_protocol"
    run([
        str(app),
        "--device",
        args.device,
        "--baud",
        args.baud,
        "--iterations",
        args.iterations,
        "--period-ms",
        args.period_ms,
    ])


if __name__ == "__main__":
    main()

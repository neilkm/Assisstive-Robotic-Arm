# Software

All software for the assistive robotic arm. The entry point for everything is `arm.sh`.

## Quick reference

```bash
# Build
Software/arm.sh build all                   # Jetson Qt app + test suite
Software/arm.sh build jetson                # Jetson Qt app only
Software/arm.sh build stm32 [protocol|echo] # STM32 firmware
Software/arm.sh build esp32  [normal|echo]  # ESP32 firmware
Software/arm.sh build tests                 # test suite only

# Flash
Software/arm.sh flash stm32 [protocol|echo] # flash STM32
Software/arm.sh flash esp32  [normal|echo]  # flash ESP32
Software/arm.sh flash all                   # flash both (production modes)

# Run & test
Software/arm.sh run                         # run Jetson Qt app
Software/arm.sh test                        # build + run tests (virtual)
Software/arm.sh test --hardware             # build + run tests (real hardware)

# Sandbox apps
Software/arm.sh sandbox build [<app>|all]
Software/arm.sh sandbox run   <app>

# Utilities
Software/arm.sh ssh                         # SSH to Jetson
Software/arm.sh setup                       # install Jetson build dependencies
Software/arm.sh clean [jetson|tests|sandbox|all]
Software/arm.sh help                        # full usage
```

## Directory layout

| Directory | Contents |
|-----------|----------|
| `Jetson/QtApp/` | Qt/QML state-machine UI application |
| `Jetson/UartProtocol/` | Jetson POSIX UART driver and protocol CLI tool |
| `Jetson/Esp32BluetoothProtocol/` | Jetson RFCOMM Bluetooth driver and protocol CLI tool |
| `Jetson/PowerDrawTest/` | CUDA power-draw stress test (Jetson only) |
| `Jetson/SystemMonitor/` | Terminal system-monitor TUI (Jetson only) |
| `STM32/` | STM32 Nucleo-F446RE motor-control firmware (PlatformIO) |
| `ESP32/` | ESP32 button-input firmware (PlatformIO / ESP-IDF) |
| `Mac/AprilTags/` | AprilTag PDF generator script (Mac) |
| `sandbox/` | Experimental apps — not production |
| `Testing/` | Unified C++ test suite (unit + integration) |

## Firmware modes

| Target | Mode | Description |
|--------|------|-------------|
| STM32 | `protocol` | Production UART packet protocol *(default)* |
| STM32 | `echo` | Text echo test |
| ESP32 | `normal` | Random button-state telemetry *(default)* |
| ESP32 | `echo` | Bluetooth UART echo test |

`flash all` always uses the production modes (`protocol` + `normal`).

## First-time Jetson setup

```bash
Software/arm.sh setup   # installs Qt6, CMake, Ninja, PlatformIO, dialout group
```

On macOS, if CMake cannot find Homebrew Qt automatically:

```bash
Software/arm.sh build jetson -qt "$(brew --prefix qt)"
```

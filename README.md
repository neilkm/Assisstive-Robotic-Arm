# Assisstive-Robotic-Arm

ECE129 capstone project: a 6-DOF assistive robotic arm.

## Subsystems

| Subsystem | Hardware | Role |
|-----------|----------|------|
| `Software/Jetson/QtApp/` | Jetson Nano | Qt/QML state-machine UI |
| `Software/STM32/` | Nucleo-F446RE | Motor control firmware (UART) |
| `Software/ESP32/` | ESP32 dev board | Button input firmware (Bluetooth) |

## Quick Start

```bash
# Build everything
./arm.sh build all

# Flash production firmware
./arm.sh flash all

# Run the UI
./arm.sh run

# Run tests (virtual, no hardware needed)
./arm.sh test

# Run tests against real hardware (on Jetson)
./arm.sh test --hardware

# SSH to Jetson
./arm.sh ssh
```

Run `./arm.sh help` for full usage.

## Directory Layout

```
Software/
  Jetson/
    QtApp/                   Qt/QML application
    UartProtocol/            Jetson UART driver and protocol tool
    Esp32BluetoothProtocol/  Jetson Bluetooth driver and tool
  STM32/                     STM32 firmware (PlatformIO)
  ESP32/                     ESP32 firmware (PlatformIO)
  sandbox/                   Experimental apps (apriltag-ik, camera-test, …)
Testing/                     Unified C++ test suite (unit + integration)
Tools/
  Mac/AprilTags/             AprilTag PDF generator (Mac)
  Jetson/PowerDrawTest/      CUDA power-draw stress test
  Jetson/SystemMonitor/      Terminal system-monitor TUI
Electrical/                  Electrical design
Mechanical/                  Mechanical design
```

# Windows Test Laptop Hardware CI

This folder contains the setup instructions, scripts, and controller app for a Windows testing laptop that runs WSL2 Ubuntu and controls the assistive robotic arm hardware test bench.

The intended setup is:

```text
GitHub repo
   |
   | repo cloned inside WSL2 Ubuntu
   v
Windows testing laptop
   |
   | WSL2 controller app
   | USB pass-through via usbipd-win
   v
Jetson Orin Nano + STM32 board + ESP32 board
```

The controller app runs continuously in WSL. It polls the configured Git branch, looks for hardware CI tags in commit messages, builds selected targets, flashes configured devices, and runs hardware-in-the-loop tests.

## Current Scope

This is the first integration layer. It provides:

- Windows/WSL setup instructions.
- USB pass-through helper scripts for Windows.
- WSL dependency setup script.
- A long-running Python controller app.
- A commit-message tag parser.
- Configurable build, flash, reset, and test commands.
- Local run logs and test report output.

The exact STM32, ESP32, and Jetson flash commands are intentionally configured in `config/hwci.yaml` on the laptop. Different boards, USB ports, probes, and firmware formats can be swapped without changing the controller code.

## Quick Start After Cloning On Windows

From PowerShell at the repo root:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\Deployment\WindowsTestLaptop\setup_windows_test_laptop.ps1
```

That script checks the Windows-side prerequisites, prints the exact missing setup steps, and points to the WSL setup command. To let it install `usbipd-win` through `winget`, run:

```powershell
.\Deployment\WindowsTestLaptop\setup_windows_test_laptop.ps1 -InstallUsbipd
```

To also run the WSL dependency setup from PowerShell after WSL is installed:

```powershell
.\Deployment\WindowsTestLaptop\setup_windows_test_laptop.ps1 -RunWslSetup
```

## Commit Message Tags

The controller only runs hardware CI when the commit message contains:

```text
[hw-ci]
```

Supported tags:

```text
[hw-ci]                         enable hardware CI for this commit
[targets:all]                   run STM32, ESP32, and Jetson stages
[targets:stm32,esp32]           run only selected targets
[build:test]                    run the configured test build commands
[build:prod]                    run the configured production build commands
[tests:smoke]                   run the smoke test suite
[tests:integration]             run the integration test suite
[tests:case:motor_can]          run a specific configured test suite/case
[flash:no]                      build and test without flashing
```

Example:

```text
Add STM32 motor watchdog [hw-ci] [targets:stm32,esp32] [build:test] [tests:smoke]
```

## Windows Prerequisites

Install on Windows:

- Windows 11 or recent Windows 10 with WSL2 support.
- WSL2 Ubuntu.
- Windows Terminal.
- Git for Windows.
- Administrator access for the first USB pass-through setup.
- `winget`, or install `usbipd-win` manually from its GitHub releases.

Install WSL2 Ubuntu from an elevated PowerShell terminal:

```powershell
wsl --install -d Ubuntu
wsl --set-default-version 2
```

Confirm Ubuntu is WSL2:

```powershell
wsl --list --verbose
```

## Enable systemd In WSL

Inside Ubuntu:

```sh
sudo sh -c 'printf "%s\n" "[boot]" "systemd=true" > /etc/wsl.conf'
```

Then restart WSL from PowerShell:

```powershell
wsl --shutdown
```

Open Ubuntu again and confirm:

```sh
systemctl --version
```

## Clone The Repo In WSL

Clone into the WSL Linux filesystem, not under `/mnt/c`, for better filesystem behavior:

```sh
mkdir -p ~/git
cd ~/git
git clone git@github.com:neilkm/Assisstive-Robotic-Arm.git
cd Assisstive-Robotic-Arm
git checkout Integration-Deployment
```

If SSH keys are not set up in WSL yet, use the HTTPS clone URL or configure a WSL-local SSH key.

## Install WSL Dependencies

From the repo root inside WSL:

```sh
bash Deployment/WindowsTestLaptop/scripts/setup_wsl.sh
```

This installs Linux build/test tools and creates a repo-local Python virtual environment at:

```text
.venv/
```

Log out and back into WSL after the setup script so group membership changes, such as `dialout`, take effect.

## Configure The Controller

Create the local hardware config:

```sh
cp Deployment/WindowsTestLaptop/config/hwci.example.yaml \
   Deployment/WindowsTestLaptop/config/hwci.yaml
```

Edit:

```text
Deployment/WindowsTestLaptop/config/hwci.yaml
```

At minimum, update:

- USB bus IDs used by `usbipd-win`.
- WSL serial device paths, such as `/dev/ttyUSB0` or `/dev/ttyACM0`.
- Flash commands for STM32 and ESP32.
- Jetson deploy/test commands.
- `controller.dry_run`, after the commands are correct.

`hwci.yaml` is intentionally ignored by Git because it is machine-specific.

## Connect The Devices

Use a powered USB hub and label every cable.

Recommended physical connections:

```text
Windows laptop
  |
  +-- powered USB hub
        |
        +-- STM32 ST-LINK USB
        +-- STM32 UART USB, if separate from ST-LINK
        +-- ESP32 USB-UART
        +-- Jetson USB serial/debug cable or USB device cable
```

The Jetson should have its own power supply. Do not rely on the Windows laptop USB port to power the Jetson.

For reliable unattended operation, add a controllable power strip or USB relay later so the controller can power-cycle stuck devices.

## Attach USB Devices To WSL

Install or verify `usbipd-win` from PowerShell:

```powershell
Deployment\WindowsTestLaptop\scripts\list_usb_devices.ps1
```

List USB devices:

```powershell
usbipd list
```

Find the bus IDs for STM32, ESP32, and Jetson. Then attach them to WSL:

```powershell
Deployment\WindowsTestLaptop\scripts\attach_usb_devices.ps1 -BusIds "1-4","1-7","1-9"
```

Inside WSL, verify:

```sh
lsusb
dmesg | tail -50
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Typical device paths:

```text
STM32 ST-LINK VCP: /dev/ttyACM0
ESP32 USB-UART:    /dev/ttyUSB0
Jetson serial:     /dev/ttyUSB1 or /dev/ttyACM1
```

USB devices usually need to be reattached after rebooting Windows, shutting down WSL, or unplugging the USB hub.

## STM32 Notes

Common flashing options:

- `openocd` with an ST-LINK probe.
- `st-flash` from `stlink-tools`.
- STM32CubeProgrammer, if installed separately.

Example OpenOCD command shape:

```sh
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program Software/build_output/path/to/firmware.elf verify reset exit"
```

Update the STM32 `flash` and `reset` commands in `hwci.yaml` after the actual firmware artifact path is known.

## ESP32 Notes

Common flashing options:

- ESP-IDF `idf.py -p /dev/ttyUSB0 flash`
- `esptool.py`

Example command shape:

```sh
idf.py -p /dev/ttyUSB0 flash
```

Update the ESP32 `flash` command in `hwci.yaml` after the production ESP-IDF project layout is finalized.

## Jetson Notes

The Jetson Orin Nano is a Linux computer, not an MCU. USB serial is useful for console logs and test control, but full application deployment is usually better over SSH using USB Ethernet, wired Ethernet, Wi-Fi, Tailscale, or a school VPN.

Recommended Jetson test-bench approach:

- Use USB serial for boot logs and health checks.
- Use SSH for copying/running Jetson application code when possible.
- Keep Jetson power independent from the laptop.

If the Jetson must be controlled only through USB serial, keep commands small and design the Jetson-side app to expose a simple serial command protocol.

## Run Once Manually

From the repo root inside WSL:

```sh
bash Deployment/WindowsTestLaptop/scripts/run_once.sh --dry-run --force
```

To run against the current local `HEAD` without dry-run, the current commit message must include `[hw-ci]`:

```sh
bash Deployment/WindowsTestLaptop/scripts/run_once.sh
```

Use `--force` only for bench bring-up when you intentionally want to run the parsed default plan without a tagged commit.

To poll continuously:

```sh
bash Deployment/WindowsTestLaptop/scripts/run_controller.sh
```

Run output is written under:

```text
Deployment/WindowsTestLaptop/runs/
```

## Install As A WSL Service

After `hwci.yaml` is configured:

```sh
bash Deployment/WindowsTestLaptop/scripts/install_controller_service.sh
```

Check status:

```sh
systemctl --user status hwci-controller
journalctl --user -u hwci-controller -f
```

Restart:

```sh
systemctl --user restart hwci-controller
```

Stop:

```sh
systemctl --user stop hwci-controller
```

## Operating Checklist

Before leaving the lab unattended:

1. Windows laptop is on wall power.
2. Windows sleep is disabled.
3. WSL service is running.
4. USB hub is powered.
5. All devices are visible in `usbipd list`.
6. Devices are attached to WSL.
7. WSL sees expected `/dev/ttyUSB*` or `/dev/ttyACM*` paths.
8. `run_once.sh --dry-run` succeeds.
9. A real `[hw-ci]` commit has been tested end to end.

## Troubleshooting

If WSL cannot see a USB device:

```powershell
usbipd list
usbipd detach --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

If serial permissions fail inside WSL:

```sh
sudo usermod -aG dialout "$USER"
```

Then restart WSL:

```powershell
wsl --shutdown
```

If the controller does not trigger, check the newest commit message:

```sh
git log -1 --pretty=%B
```

It must include `[hw-ci]`.

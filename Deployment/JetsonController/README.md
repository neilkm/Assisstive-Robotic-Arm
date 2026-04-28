# Jetson Hardware CI Controller

This folder contains the setup instructions, scripts, and controller app for using the Jetson Orin Nano as the always-on hardware test controller.

The intended setup is:

```text
GitHub repo
   |
   | repo cloned on Jetson
   v
Jetson Orin Nano
   |
   | native Ubuntu controller app
   | direct USB serial/programming links
   v
STM32 board + ESP32 board
```

The Jetson runs the controller continuously. It polls the configured Git branch, looks for hardware CI tags in commit messages, builds selected targets, flashes configured devices over USB, and runs hardware-in-the-loop tests.

## Current Scope

This is the first Jetson integration layer. It provides:

- Jetson setup instructions.
- Direct USB connection guidance for STM32 and ESP32.
- A long-running Python controller app.
- A commit-message tag parser.
- Configurable build, flash, reset, and test commands.
- A user `systemd` service.
- Local run logs and test report output.

The exact STM32 and ESP32 flash commands are intentionally configured in `config/hwci.yaml` on the Jetson. Different boards, USB ports, probes, and firmware formats can be swapped without changing the controller code.

## Quick Start On The Jetson

From the repo root on the Jetson:

```sh
bash Deployment/JetsonController/scripts/setup_jetson.sh
```

Then create the local machine config:

```sh
cp Deployment/JetsonController/config/hwci.example.yaml \
   Deployment/JetsonController/config/hwci.yaml
```

Edit:

```text
Deployment/JetsonController/config/hwci.yaml
```

Run a dry-run:

```sh
bash Deployment/JetsonController/scripts/run_once.sh --dry-run --force
```

Install the always-running service after the config is correct:

```sh
bash Deployment/JetsonController/scripts/install_controller_service.sh
```

## Connecting To The Jetson For Setup

Preferred options:

1. Ethernet to the same network as the setup machine.
2. Direct Ethernet cable with static IPs.
3. USB device-mode networking, if enabled on the Jetson image.
4. Serial console only as a fallback.

### Same Network Ethernet

On the Jetson:

```sh
hostname -I
sudo systemctl enable --now ssh
```

Connect from another machine:

```sh
ssh <jetson-user>@<jetson-ip>
```

### Direct Ethernet Static IP

On the Jetson:

```sh
sudo ip addr add 10.10.10.2/24 dev eth0
sudo ip link set eth0 up
sudo systemctl enable --now ssh
```

On the other machine, set its Ethernet adapter to:

```text
IP address: 10.10.10.1
Subnet:     255.255.255.0
Router:     blank
```

Then connect:

```sh
ssh <jetson-user>@10.10.10.2
```

### USB Device-Mode Networking

Some Jetson images expose a USB network interface when connected through the recovery/device USB port. If available, the Jetson commonly appears at:

```text
192.168.55.1
```

Connect with:

```sh
ssh <jetson-user>@192.168.55.1
```

If that does not work, use Ethernet. USB device networking depends on the Jetson image and cable/port.

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

## Clone The Repo On The Jetson

Clone into the Jetson Linux filesystem:

```sh
mkdir -p ~/git
cd ~/git
git clone git@github.com:neilkm/Assisstive-Robotic-Arm.git
cd Assisstive-Robotic-Arm
git checkout Integration-Deployment
```

If SSH keys are not set up on the Jetson yet, use the HTTPS clone URL or configure a Jetson-local SSH key.

## Install Jetson Dependencies

From the repo root on the Jetson:

```sh
bash Deployment/JetsonController/scripts/setup_jetson.sh
```

This installs Linux build/test tools and creates a repo-local Python virtual environment at:

```text
.venv/
```

Log out and back in after the setup script so group membership changes, such as `dialout` and `plugdev`, take effect.

## Configure The Controller

Create the local hardware config:

```sh
cp Deployment/JetsonController/config/hwci.example.yaml \
   Deployment/JetsonController/config/hwci.yaml
```

Edit:

```text
Deployment/JetsonController/config/hwci.yaml
```

At minimum, update:

- STM32 and ESP32 serial paths.
- Flash commands for STM32 and ESP32.
- Any reset commands.
- `controller.dry_run`, after the commands are correct.

`hwci.yaml` is intentionally ignored by Git because it is machine-specific.

## Connect The Devices

Use a powered USB hub if the Jetson cannot provide stable current to both boards.

Recommended physical connections:

```text
Jetson Orin Nano
  |
  +-- powered USB hub
        |
        +-- STM32 ST-LINK USB
        +-- STM32 UART USB, if separate from ST-LINK
        +-- ESP32 USB-UART
```

The Jetson should have its own power supply. Do not power the Jetson from the STM32, ESP32, or a weak hub.

For reliable unattended operation, add a controllable power strip or USB relay later so the controller can power-cycle stuck devices.

## Verify USB Devices

On the Jetson:

```sh
lsusb
dmesg | tail -50
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Typical device paths:

```text
STM32 ST-LINK VCP: /dev/ttyACM0
ESP32 USB-UART:    /dev/ttyUSB0
```

Device numbering can change if boards are unplugged or boot order changes. For long-term reliability, add `udev` rules that create stable names such as:

```text
/dev/assistive_stm32
/dev/assistive_esp32
```

The `setup_jetson.sh` script creates a placeholder udev rule file at:

```text
/etc/udev/rules.d/99-assistive-arm.rules
```

Edit it once the board vendor/product IDs and serial numbers are known.

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

The Jetson target is the same machine running the controller. The default Jetson target commands build the existing Rust module through the repo Makefiles. If later the production app becomes a long-running local process, add service restart and health-check commands to `hwci.yaml`.

## Run Once Manually

From the repo root on the Jetson:

```sh
bash Deployment/JetsonController/scripts/run_once.sh --dry-run --force
```

To run against the current local `HEAD` without dry-run, the current commit message must include `[hw-ci]`:

```sh
bash Deployment/JetsonController/scripts/run_once.sh
```

Use `--force` only for bench bring-up when you intentionally want to run the parsed default plan without a tagged commit.

To poll continuously:

```sh
bash Deployment/JetsonController/scripts/run_controller.sh
```

Run output is written under:

```text
Deployment/JetsonController/runs/
```

## Install As A User Service

After `hwci.yaml` is configured:

```sh
bash Deployment/JetsonController/scripts/install_controller_service.sh
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

1. Jetson is on wall power.
2. Jetson SSH is reachable.
3. Controller service is running.
4. USB hub is powered, if used.
5. STM32 and ESP32 are visible in `lsusb`.
6. Jetson sees expected `/dev/ttyUSB*` or `/dev/ttyACM*` paths.
7. `run_once.sh --dry-run --force` succeeds.
8. A real `[hw-ci]` commit has been tested end to end.

## Troubleshooting

If serial permissions fail:

```sh
sudo usermod -aG dialout,plugdev "$USER"
```

Then log out and back in.

If a USB serial device disappears:

```sh
lsusb
dmesg | tail -80
sudo udevadm control --reload-rules
sudo udevadm trigger
```

If the controller does not trigger, check the newest commit message:

```sh
git log -1 --pretty=%B
```

It must include `[hw-ci]`.

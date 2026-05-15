# Software Scripts

Shared helper scripts for building, running, flashing, and connecting to project
software targets. Run these commands from the repository root unless noted
otherwise.

## Jetson Qt App

Install Jetson Qt build requirements:

```bash
Software/scripts/install_jetson_qt_requirements.sh
```

This installs Qt/CMake/Ninja dependencies, PlatformIO, and adds the install user
to the `dialout` group when available. Log out and back in after the first
`dialout` group update before flashing boards over USB serial.

Build all Jetson Qt apps:

```bash
Software/scripts/build_jetson_qt_apps.sh
```

Build artifacts are written to `builds/Jetson`. App executables are under
`builds/Jetson/apps/<app_name>/`, and QTest executables are under
`builds/Jetson/tests/<test_name>/`.

Extra CMake configure arguments are passed through to `cmake -S ... -B ...`.
For example:

```bash
Software/scripts/build_jetson_qt_apps.sh -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

Run the StateMachine UI:

```bash
Software/scripts/run_jetson_qt_state_machine_ui.sh
```

If the app is not already built, the run script builds it first. On Jetson Linux
it targets the attached local display by default. Override the display when
needed:

```bash
JETSON_QT_DISPLAY=:1 Software/scripts/run_jetson_qt_state_machine_ui.sh
```

Clean the Jetson Qt build directory:

```bash
Software/scripts/clean_jetson_qt_build.sh
```

## ESP32 App

Build and flash the ESP32 PlatformIO project:

```bash
Software/scripts/build_and_flash_esp32_app.sh
```

The script runs from `Software/ESP32App` internally and executes:

```bash
pio run
pio run -t upload
```

Any arguments passed to the script are forwarded to both PlatformIO commands.
For example, to select a serial upload port:

```bash
Software/scripts/build_and_flash_esp32_app.sh --upload-port /dev/ttyUSB0
```

## Jetson SSH

Open an SSH session to the Jetson using values from `secrets.env` or the
environment:

```bash
Software/scripts/ssh_jetson.sh
```

Required variables:

- `JETSON_SSH_HOST`
- `JETSON_SSH_USER`
- `JETSON_SSH_PASSWORD`

Extra arguments are passed through to `ssh`, for example:

```bash
Software/scripts/ssh_jetson.sh -L 5901:localhost:5901
```

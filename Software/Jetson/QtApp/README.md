# Jetson

Cross-platform Qt/CMake workspace for robotic-arm UI applications. It is intended to build on macOS during development and on Jetson Linux for deployment.

## Layout

- `Components/`: backend modules. Each component owns the sources and headers for one backend class or module and exports a CMake target alias named `JetsonQtApp::<library_name>`.
- `Common/UICommon/`: shared QML components, UI utility C++ code, and shared image assets.
- `Common/Theme/`: shared theme QML files.
- `UserInterface/`: UI pages, page controller classes, and controller QTests.
- `Apps/`: application entry-point folders. Each app folder contains the app `main.cpp`.
- `cmake/JetsonQtAppHelpers.cmake`: helper functions for adding libraries and apps consistently.
- `../../builds/Jetson/`: centralized CMake build tree and generated app/test executables.

Libraries are ordinary CMake targets. Any app or library can use another library by linking its exported target, provided the dependency chain is satisfied.

## Current App

`StateMachine_UI` is a Qt Quick UI for the robotic-arm cooking state machine. It preserves the same states, actions, and state images while replacing OpenCV window drawing with Qt/QML.

Controls:

- `Arrow keys`: move the highlighted action
- `Enter`: trigger the highlighted action and transition states
- `Esc`: quit

## Build

Install Qt first.

macOS with Homebrew:

```bash
brew install qt cmake ninja
```

Jetson Ubuntu:

```bash
Software/scripts/install_jetson_qt_requirements.sh
```

The Jetson requirements script also installs PlatformIO so the Jetson can build
and flash MCU firmware from a PlatformIO project:

```bash
Software/scripts/build_and_flash_esp32_app.sh
```

If `pio` is not found immediately after installation, open a new shell or add
`$HOME/.local/bin` to `PATH`.

The script adds the install user to the `dialout` group when that group exists.
Log out and back in before flashing an MCU over USB serial for the first time.

If your Jetson image only provides Qt 5, install `qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev qml-module-qtqml-workerscript` instead. The project accepts either Qt 6 or Qt 5.

Configure and build:

```bash
Software/scripts/build_jetson_qt_apps.sh
```

The script configures CMake from `Software/Jetson` and writes all generated
Jetson artifacts under `builds/Jetson`. App executables are placed under
`builds/Jetson/apps/<app_name>/`, and QTest executables are placed under
`builds/Jetson/tests/<test_name>/`.

Run tests:

```bash
ctest --test-dir builds/Jetson
```

Run:

```bash
Software/scripts/run_jetson_qt_state_machine_ui.sh
```

On Jetson Linux, the run script targets the Jetson's attached desktop display by
default, even when launched from an SSH session. It prefers the local Wayland
socket when one is available and falls back to the local X11 display socket. If a
Jetson image uses a different local X11 display number, override it when
launching:

```bash
JETSON_QT_DISPLAY=:1 Software/scripts/run_jetson_qt_state_machine_ui.sh
```

Clean:

```bash
Software/scripts/clean_jetson_qt_build.sh
```

On macOS, if CMake cannot find Homebrew Qt automatically, configure with:

```bash
cmake -S Software/Jetson -B builds/Jetson -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

# JetsonQtApp

Cross-platform Qt/CMake workspace for robotic-arm UI applications. It is intended to build on macOS during development and on Jetson Linux for deployment.

## Layout

- `libs/`: shared libraries. Each library exports a CMake target alias named `JetsonQtApp::<library_name>`.
- `apps/`: application folders. Each app can have an app-specific library and a small executable entry point.
- `cmake/JetsonQtAppHelpers.cmake`: helper functions for adding libraries and apps consistently.

Libraries are ordinary CMake targets. Any app or library can use another library by linking its exported target, provided the dependency chain is satisfied.

## Current App

`StateMachine_UI` is a Qt Widgets clone of `Software/sandbox/jetson-opencv-state-ui`. It preserves the same states, actions, keyboard behavior, and state images while replacing OpenCV window drawing with Qt.

Controls:

- `Arrow keys`: move the highlighted action
- `Enter`: trigger the highlighted action and transition states
- `i`: reset to `Init`
- `j` / `k`: alternate action navigation keys
- `q` or `Esc`: quit

## Build

Install Qt first.

macOS with Homebrew:

```bash
brew install qt cmake ninja
```

Jetson Ubuntu:

```bash
Software/JetsonQtApp/scripts/install_requirements.sh
```

If your Jetson image only provides Qt 5, install `qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev` instead. The project accepts either Qt 6 or Qt 5.

Configure and build:

```bash
Software/JetsonQtApp/scripts/build_all_apps.sh
```

Run:

```bash
Software/JetsonQtApp/scripts/run_StateMachine_UI.sh
```

On Jetson Linux, the run script targets the Jetson's attached desktop display by
default, even when launched from an SSH session. It prefers the local Wayland
socket when one is available and falls back to the local X11 display socket. If a
Jetson image uses a different local X11 display number, override it when
launching:

```bash
JETSON_QT_DISPLAY=:1 Software/JetsonQtApp/scripts/run_StateMachine_UI.sh
```

Clean:

```bash
Software/JetsonQtApp/scripts/clean_all_builds.sh
```

On macOS, if CMake cannot find Homebrew Qt automatically, configure with:

```bash
cmake -S Software/JetsonQtApp -B Software/JetsonQtApp/build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

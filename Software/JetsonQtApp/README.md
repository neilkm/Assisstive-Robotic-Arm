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
sudo apt update
sudo apt install -y build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev
```

If your Jetson image only provides Qt 5, install `qtbase5-dev qtdeclarative5-dev` instead. The project accepts either Qt 6 or Qt 5.

Configure and build:

```bash
Software/JetsonQtApp/scripts/build_all_apps.sh
```

Run:

```bash
Software/JetsonQtApp/scripts/run_StateMachine_UI.sh
```

Clean:

```bash
Software/JetsonQtApp/scripts/clean_all_builds.sh
```

On macOS, if CMake cannot find Homebrew Qt automatically, configure with:

```bash
cmake -S Software/JetsonQtApp -B Software/JetsonQtApp/build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

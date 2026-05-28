# jetson-opencv-state-ui

Standalone OpenCV/C++ sandbox app for Jetson Nano. Prototype of the cooking state-machine UI rendered in an OpenCV window. Superseded by the Qt/QML `StateMachine_UI` in `Software/Jetson/QtApp/`.

## Controls

| Key | Action |
|-----|--------|
| Arrow keys | Move highlighted action |
| Enter | Trigger action / transition state |
| `i` | Reset to Init |
| `j` / `k` | Alternate action navigation |
| `q` / Esc | Quit |

## Build and run

```bash
Software/arm.sh sandbox build opencv-state-ui   # CMake build → builds/sandbox/opencv-state-ui/
Software/arm.sh sandbox run   opencv-state-ui   # launch app
```

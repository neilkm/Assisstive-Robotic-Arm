# QtApp

Qt/CMake workspace for Jetson applications. Builds on macOS (development) and Jetson Linux (deployment).

## Layout

```
Components/           Backend C++ modules, each exported as JetsonQtApp::<name>
  StateMachine/       Cooking state-machine logic (no Qt dependency)
  ObjectDetection/    Object detection component
Common/
  UICommon/           Shared QML components, UI utilities, image assets
UserInterface/        QML pages + C++ page controllers + QTests
  StateMachine/       Main state-machine UI page
  TagVisualization/   AprilTag visualization page
Apps/
  StateMachine_UI/    Application entry point (main.cpp)
cmake/
  JetsonQtAppHelpers.cmake  add_jetson_library() / add_jetson_app() / add_jetson_qtest()
configs/
  DH_Table.md         Robot arm DH parameters
```

## StateMachine_UI controls

- Arrow keys — move highlighted action
- Enter — trigger action / transition state
- Esc — quit

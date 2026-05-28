# Software/scripts

Helper scripts invoked by `arm.sh` and usable directly. Run from the repository root.

| Script | Purpose |
|--------|---------|
| `build_jetson_qt_apps.sh` | Configure + build the Jetson Qt app |
| `run_jetson_qt_state_machine_ui.sh` | Launch StateMachine_UI (builds first if needed) |
| `clean_jetson_qt_build.sh` | Remove `builds/Jetson` |
| `build_and_flash_esp32_app.sh [normal\|echo]` | Build + flash an ESP32 environment |
| `install_jetson_qt_requirements.sh` | Install Qt6, CMake, Ninja, PlatformIO on Jetson Ubuntu |
| `ssh_jetson.sh` | SSH to Jetson using credentials from `secrets.env` |

The preferred entry point is `arm.sh` at the repository root — see `arm.sh help`.

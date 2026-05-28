# jetson-camera-test

Standalone C++/OpenCV sandbox app for Jetson Nano. Opens a camera-selection screen listing all detected devices, then shows a live video feed for the chosen camera.

Supports V4L2 cameras (`/dev/video*`) and Jetson CSI cameras via `nvarguscamerasrc` (sensor IDs 0 and 1). CSI support requires the NVIDIA JetPack camera stack.

## Controls

| Key | Action |
|-----|--------|
| Arrow keys | Move through camera list |
| Enter | Open selected camera |
| `r` | Refresh camera list |
| `b` | Back to camera selection |
| `q` / Esc | Quit |

## Build and run

```bash
Software/arm.sh sandbox build camera-test   # CMake build → builds/sandbox/camera-test/
Software/arm.sh sandbox run   camera-test   # launch app
```

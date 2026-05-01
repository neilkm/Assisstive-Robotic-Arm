# Jetson Camera Test

Small standalone C++/OpenCV sandbox app for NVIDIA Jetson Nano.

It opens a single UI window with:
- a launch screen that lists detected cameras,
- keyboard navigation to choose a camera,
- a live video view for the selected device.

The app is aimed at Jetson Nano and supports:
- V4L2 cameras exposed as `/dev/video*`,
- Jetson CSI cameras available through `nvarguscamerasrc` on sensor IDs `0` and `1`.

CSI camera support depends on the NVIDIA JetPack camera stack providing `nvarguscamerasrc`.
The install script verifies that plugin and warns if it is missing.

## Controls

- `Arrow keys`: move through the camera list
- `Enter`: open the selected camera
- `r`: refresh the detected camera list
- `b`: leave the live view and return to camera selection
- `q` or `Esc`: quit

## Jetson Nano Setup

From this directory:

```bash
chmod +x install.sh build.sh run.sh
./install.sh
./run.sh
```

`install.sh` installs the required Ubuntu packages and then builds the project with CMake.

## Build Manually

```bash
./build.sh
./run.sh
```

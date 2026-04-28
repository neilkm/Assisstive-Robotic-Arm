# Jetson OpenCV State UI

Small standalone OpenCV/C++ sandbox app for NVIDIA Jetson Nano.

It opens a UI window that shows:
- the current state,
- the list of allowed actions for that state,
- one highlighted action at a time.

## Controls

- `Up` / `Down`: cycle through the configured states
- `j` / `k`: move the highlighted action within the current state's allowed actions
- `q` or `Esc`: quit

## States

The app includes these states and actions:

- `Init`
  - `select spice`
  - `select utensil`
- `Selecting spice`
  - `spice 1` through `spice 9`
- `Spice selected`
  - `shake into pot`
  - `put down`
- `Shaking spice into pot`
  - `more spice`
  - `less spice`
  - `put down`
- `Selecting utensil`
  - `utensil 1` through `utensil 5`
- `Utensil selected`
  - `use utensil`
  - `put down`
- `Using utensil`
  - `stir faster`
  - `stir slower`
  - `put down`

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

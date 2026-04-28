# Jetson OpenCV State UI

Small standalone OpenCV/C++ sandbox app for NVIDIA Jetson Nano.

It opens a UI window that shows:
- the current state,
- the list of allowed actions for that state,
- one highlighted action at a time.

The UI now behaves as a state machine:
- arrow keys move the highlighted action,
- `Enter` triggers the highlighted action and transitions to the next state,
- `i` resets the machine to `Init`.

Text is rendered in Arial when `Arial.ttf` is available on the system. If Arial is not installed, the app falls back to OpenCV's built-in font so it still runs.

## Controls

- `Arrow keys`: move the highlighted action
- `Enter`: trigger the highlighted action and transition states
- `i`: reset to `Init`
- `j` / `k`: alternate action navigation keys
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

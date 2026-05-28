# SystemMonitor

Terminal TUI for monitoring a Jetson or Linux host from an SSH session. Shows host identity, SSH connections, logged-in sessions, memory, swap, load average, uptime, disk usage, Jetson power draw, thermal zones, and top CPU processes.

Uses only Python standard-library modules and Linux system tools. Jetson power values are read from NVIDIA sysfs sensors or `tegrastats` when available.

## Install

```bash
Software/Jetson/SystemMonitor/install.sh
```

Installs `System-Monitor-NNK` to `/usr/local/bin`. Override the install directory:

```bash
INSTALL_DIR="$HOME/.local/bin" Software/Jetson/SystemMonitor/install.sh
```

## Run

```bash
System-Monitor-NNK
```

Or via arm.sh:

```bash
Software/arm.sh tools run system-monitor
```

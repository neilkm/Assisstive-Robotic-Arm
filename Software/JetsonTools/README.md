# JetsonTools

Small command-line tools for monitoring and operating Jetson/Linux hosts.

## System-Monitor-NNK

`System-Monitor-NNK` is a terminal TUI for watching a server from an SSH session.
It shows:

- host identity and current `user@ip`
- established SSH TCP connections
- logged-in shell sessions
- memory and swap usage
- load average, uptime, and root disk usage
- Jetson power draw and power mode when available
- thermal zones and top CPU processes

The app uses only Python standard-library modules and Linux system tools. Jetson
power values are read from NVIDIA sysfs sensors or `tegrastats` when available.

Install:

```sh
./install.sh
```

Run after installation:

```sh
System-Monitor-NNK
```

## SSH Helper

`ssh_jetson.sh` opens an SSH session to the Jetson through Tailscale, so it does
not depend on the campus network IP address.

```sh
Software/JetsonTools/ssh_jetson.sh
```

Defaults:

```sh
JETSON_SSH_USER=jetson
JETSON_SSH_HOST=jetson-arm.tail7a7a89.ts.net
```

Override either value if needed:

```sh
JETSON_SSH_HOST=100.89.146.99 Software/JetsonTools/ssh_jetson.sh
```

By default the installer writes to `/usr/local/bin`. To install somewhere else:

```sh
INSTALL_DIR="$HOME/.local/bin" ./install.sh
```

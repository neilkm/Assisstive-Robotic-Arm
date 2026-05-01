#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    python3-pip \
    python3-venv \
    pkg-config \
    libxkbcommon-dev \
    libxcb-cursor0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-render-util0 \
    libxcb-xinerama0 \
    qt6-base-dev \
    qt6-declarative-dev \
    qt6-multimedia-dev \
    qt6-wayland \
    qml6-module-qtqml \
    qml6-module-qtqml-workerscript \
    qml6-module-qtquick \
    qml6-module-qtquick-window \
    qml6-module-qtmultimedia

python3 -m pip install --user --upgrade platformio

install_user="${SUDO_USER:-${USER}}"
if getent group dialout >/dev/null 2>&1; then
    sudo usermod -a -G dialout "${install_user}"
fi

if ! command -v pio >/dev/null 2>&1; then
    cat <<'EOF'

PlatformIO was installed under the current user's Python scripts directory.
Open a new shell, or add this to your shell profile before running pio:

    export PATH="$HOME/.local/bin:$PATH"

EOF
fi

cat <<EOF

If this is the first time ${install_user} was added to the dialout group, log
out and back in before flashing an MCU over USB serial.

EOF

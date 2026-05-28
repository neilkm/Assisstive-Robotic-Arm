# ESP32

PlatformIO firmware for the ESP32 button-input board (Bluetooth Classic SPP).

## Environments

| Environment | Mode flag | Description |
|-------------|-----------|-------------|
| `esp32dev` | _(none)_ | Random button telemetry — production transport test |
| `esp32dev_bluetooth_echo_test` | `-DESP32_BT_ECHO_TEST` | Bluetooth UART echo test |

Build or flash via `arm.sh`:

```bash
./arm.sh build esp32 normal
./arm.sh flash esp32 normal
./arm.sh flash esp32 echo
```

Or directly with PlatformIO:

```bash
pio run -d Software/ESP32 -e esp32dev
pio run -d Software/ESP32 -e esp32dev -t upload
```

## Layout

```
include/esp32_button_packet.h   Shared packet framing (magic 0xB7 0x32, CRC-16)
src/main.c                      Firmware source
scripts/flash_random_button_test.py   Flash helper
```

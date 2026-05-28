Useful commands:

#monitor serial port
pio device monitor -p /dev/ttyUSB0 -b 115200 --filter time

#build clean upload
pio run 
pio run -t clean
pio run -t upload

#build and upload from the repository root
Software/scripts/build_and_flash_esp32_app.sh

## Bluetooth random button test firmware

This app advertises a Bluetooth Classic SPP service named `ArmESP32Buttons`.
The Jetson pairs/trusts that device and binds it to `/dev/rfcomm0` before
running the listener in `Software/Jetson/Esp32BluetoothProtocol`.

This is currently a transport test. It does not read physical GPIOs yet; each
fresh state packet contains a new random six-bit button mask. If an ACK is not
received, the firmware retransmits the same sequence and mask.

Build:

```bash
pio run -d Software/ESP32 -e esp32dev
```

Upload:

```bash
pio run -d Software/ESP32 -e esp32dev -t upload
```

Run the Jetson-side full test:

```bash
Software/Jetson/Esp32BluetoothProtocol/tests/test_esp32_bluetooth_protocol.py --device /dev/rfcomm0
```

Flash only the active random button test:

```bash
Software/ESP32/scripts/flash_random_button_test.py
```

## Bluetooth UART echo test firmware

This separate test mode validates interactive Bluetooth UART text traffic.
After pairing, the ESP32 sends a banner over Bluetooth. The Jetson test script
prompts for a string, sends it over `/dev/rfcomm0`, and expects the ESP32 to
echo it as `Rx [message]`.

Run the full Jetson-side echo test:

```bash
Software/Jetson/Esp32BluetoothProtocol/tests/test_bluetooth_echo.py --device /dev/rfcomm0
```

Build or upload the echo firmware directly:

```bash
pio run -d Software/ESP32 -e esp32dev_bluetooth_echo_test
pio run -d Software/ESP32 -e esp32dev_bluetooth_echo_test -t upload
```

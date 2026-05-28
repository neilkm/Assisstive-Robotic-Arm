# Assisstive-Robotic-Arm

ECE129 Capstone project repository for the assistive robotic arm software, electrical, and mechanical assets.

## Functional Directory Structure

- `Software/`: embedded and host-side software. 
- `Mechanical/`: mechanical design/docs.
- `Electrical/`: electrical design/docs.

## Jetson/STM32 UART Protocol

The active Jetson-to-STM32 serial protocol lives in:

- `Software/STM32/include/packet.h`: shared packet framing, CRC, and payload helpers.
- `Software/STM32/src/uart.c`: interrupt-driven STM32 USART2 byte transport.
- `Software/STM32/src/test.c`: STM32 UART test harness with a conditional protocol main.
- `Software/Jetson/UartProtocol/`: Jetson POSIX UART driver, protocol exerciser, and tests.

The old standalone `Software/Common/UART_Driver` experiment and empty Qt
component placeholders (`StmUart`, `EspBle`, and `PoseDetection`) were removed
so the tree only carries maintained source files.

### Packet Format

All multi-byte floating-point payload values are encoded explicitly as
little-endian IEEE-754 `float` values. Frames are binary and use this layout:

```text
magic[0]      0xA5
magic[1]      0x5A
version       0x01
type          0x01 actual state, 0x02 desired state
sequence      uint8_t sequence counter
payload_len   uint8_t payload byte count
payload       packet-specific float payload
crc16         little-endian CRC-16/CCITT over version/type/sequence/len/payload
```

Packet types:

- `actual state`: STM32 to Jetson, 7 actual joint-angle floats plus 1 force-sensor float.
- `desired state`: Jetson to STM32, 7 desired joint-angle floats.

The parser can resynchronize on the magic bytes and rejects frames with the
wrong version, oversized payloads, or mismatched CRC.

### STM32 Test Firmware

The default STM32 PlatformIO environment still builds the text echo UART test:

```bash
pio run -d Software/STM32 -e nucleo_f446re
```

The protocol test firmware is enabled through the dedicated environment:

```bash
pio run -d Software/STM32 -e nucleo_f446re_uart_protocol
pio run -d Software/STM32 -e nucleo_f446re_uart_protocol -t upload
```

The protocol firmware uses USART2 at 115200 baud. On the Nucleo-F446RE:

- `PA2` is USART2 TX.
- `PA3` is USART2 RX.
- The ST-Link USB virtual COM port also exposes USART2 and usually appears as
  `/dev/ttyACM0` on Jetson/Linux.

For the protocol test main, the STM32 continuously sends actual-state frames.
When it receives a desired-state frame, it stores those values as desired joint
angles. For hardware test visibility, it mirrors the last received desired
angles into subsequent actual-state telemetry and sets the force value to the
received desired packet sequence. The Nucleo user LED on `PA5` toggles each
time an actual-state frame is queued, which confirms the protocol firmware is
running and transmitting.

### Jetson Protocol App

Build the Jetson protocol app and local packet test:

```bash
cmake -S Software/Jetson/UartProtocol -B builds/JetsonUartProtocol
cmake --build builds/JetsonUartProtocol
```

Run the local packet round-trip test without hardware:

```bash
builds/JetsonUartProtocol/packet_roundtrip_test
```

Run the end-to-end hardware test over the Nucleo USB virtual COM port:

```bash
Software/Jetson/UartProtocol/tests/test_uart_protocol.py --device /dev/ttyACM0 --rx-hex
```

Use `/dev/ttyTHS1` only when the Jetson hardware UART pins are wired directly to
the STM32 USART2 pins:

- STM32 `PA2`/TX to Jetson RX.
- STM32 `PA3`/RX to Jetson TX.
- STM32 ground to Jetson ground.

The Jetson protocol app prints each desired-state frame it sends and each
actual-state frame it decodes. Its final summary reports raw RX byte count,
valid decoded frame count, actual-state frame count, and non-actual frame count.
A successful bidirectional hardware test shows mirrored desired angles returning
as actual angles with `force` equal to the transmitted desired packet sequence.

Recent validated hardware result over `/dev/ttyACM0`:

```text
sent_desired=20
rx_bytes=1137
valid_frames=28
received_actual=28
non_actual_frames=0
```

During that run, desired sequences `1` through `19` were echoed back by the STM
as actual-state angles with matching force sequence values, verifying both
Jetson-to-STM32 and STM32-to-Jetson packet transfer.

## Jetson/ESP32 Bluetooth Button Protocol

The ESP32-to-Jetson button protocol lives in:

- `Software/ESP32/include/esp32_button_packet.h`: shared packet framing, CRC,
  six-button payload, and ACK helpers.
- `Software/ESP32/src/main.c`: ESP32 Bluetooth Classic SPP random button-state
  test firmware.
- `Software/Jetson/Esp32BluetoothProtocol/`: Jetson RFCOMM serial listener,
  ACK writer, pairing helper, and tests.

The ESP32 advertises as `ArmESP32Buttons` and exposes a Bluetooth Classic SPP
service. The Jetson pairs/trusts that device, binds it to `/dev/rfcomm0`, then
uses the RFCOMM device like a UART. Once paired and trusted, Linux/BlueZ can
reconnect automatically when both boards are powered and the `rfcomm` binding is
active.

### Button Packet Format

Frames use the same header/CRC shape as the STM UART protocol, with a smaller
payload:

```text
magic[0]      0xB7
magic[1]      0x32
version       0x01
type          0x10 button state, 0x11 ACK
sequence      uint8_t sequence counter
payload_len   uint8_t payload byte count
payload       button state mask for state packets, empty for ACK packets
crc16         little-endian CRC-16/CCITT over version/type/sequence/len/payload
```

Button-state payload bit `0` maps to button 1 and bit `5` maps to button 6. For
now, the ESP32 test firmware sends a new random six-bit mask for each fresh
state packet instead of reading physical GPIOs.

### ESP32 Button Firmware

Build or upload the ESP32 firmware:

```bash
pio run -d Software/ESP32 -e esp32dev
pio run -d Software/ESP32 -e esp32dev -t upload
```

Or flash the current active random button telemetry test with:

```bash
Software/ESP32/scripts/flash_random_button_test.py
```

The firmware sends one random state packet, waits for the Jetson ACK for that
sequence, and retransmits the same packet if the ACK does not arrive within
250 ms.

### Jetson Bluetooth Test

Build the Jetson listener and local packet test:

```bash
cmake -S Software/Jetson/Esp32BluetoothProtocol -B builds/JetsonEsp32BluetoothProtocol
cmake --build builds/JetsonEsp32BluetoothProtocol
ctest --test-dir builds/JetsonEsp32BluetoothProtocol --output-on-failure
```

Pair and bind the ESP32 to an RFCOMM device on the Jetson:

```bash
Software/Jetson/Esp32BluetoothProtocol/scripts/pair_esp32_bluetooth.sh
```

Run the hardware listener:

```bash
builds/JetsonEsp32BluetoothProtocol/jetson_esp32_bluetooth_protocol --device /dev/rfcomm0 --rx-hex
```

Or run the full-test wrapper that builds, optionally flashes, pairs, listens,
and prints a parsed summary of random masks and ACKs:

```bash
Software/Jetson/Esp32BluetoothProtocol/tests/test_esp32_bluetooth_protocol.py --device /dev/rfcomm0 --rx-hex
```

### Bluetooth UART Echo Test

The echo test is a separate ESP32 firmware mode for validating that Bluetooth is
the only runtime link between the Jetson and ESP32. It sends a test-runner
banner over Bluetooth UART after pairing. The Jetson terminal prompts for a
string, sends it over `/dev/rfcomm0`, and expects the ESP32 to echo it back as
`Rx [message]`.

Run the full echo test from the Jetson:

```bash
Software/Jetson/Esp32BluetoothProtocol/tests/test_bluetooth_echo.py --device /dev/rfcomm0
```

To build or upload the echo firmware directly:

```bash
pio run -d Software/ESP32 -e esp32dev_bluetooth_echo_test
pio run -d Software/ESP32 -e esp32dev_bluetooth_echo_test -t upload
```

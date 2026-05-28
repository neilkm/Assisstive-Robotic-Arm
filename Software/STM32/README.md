# STM32App

PlatformIO firmware project for the STM32 Nucleo-F446RE.

The current app uses the shared UART ring-buffer driver in `Software/Common/UART_Driver`
and binds it to USART2, which is connected to the Nucleo ST-LINK USB virtual COM port.

## Build

```sh
pio run
```

## Flash

```sh
pio run --target upload
```

## Jetson PlatformIO setup

Jetson is Linux ARM64. PlatformIO's STM32 platform may fail to install its default
`toolchain-gccarmnoneeabi` package for `linux_aarch64`. Use the Jetson-specific
environment with the system ARM embedded toolchain:

```sh
sudo apt update
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi
./scripts/setup_jetson_platformio_toolchain.sh
pio run -e nucleo_f446re_jetson -t upload
```

## Serial monitor

```sh
pio device monitor --baud 115200
```

Send a line ending with Enter. The board echoes it as:

```text
received: <your string>
```

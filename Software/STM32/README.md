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

## Serial monitor

```sh
pio device monitor --baud 115200
```

Send a line ending with Enter. The board echoes it as:

```text
received: <your string>
```

#ifndef JETSON_BLUETOOTH_UART_DRIVER_H
#define JETSON_BLUETOOTH_UART_DRIVER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>

class BluetoothUartDriver {
public:
    BluetoothUartDriver();
    ~BluetoothUartDriver();

    BluetoothUartDriver(const BluetoothUartDriver&) = delete;
    BluetoothUartDriver& operator=(const BluetoothUartDriver&) = delete;

    bool openPort(const std::string& device, int baudrate, std::string* error);
    void closePort();
    bool isOpen() const;

    ssize_t readBytes(uint8_t* data, size_t len, int timeout_ms, std::string* error);
    bool writeAll(const uint8_t* data, size_t len, std::string* error);

private:
    int fd_;
};

#endif

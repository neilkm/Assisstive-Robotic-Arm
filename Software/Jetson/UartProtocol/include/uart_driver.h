#ifndef JETSON_UART_DRIVER_H
#define JETSON_UART_DRIVER_H

#include <cstddef>
#include <cstdint>
#include <string>

class UartDriver {
public:
    UartDriver();
    ~UartDriver();

    UartDriver(const UartDriver&) = delete;
    UartDriver& operator=(const UartDriver&) = delete;

    bool openPort(const std::string& device, int baudrate, std::string* error);
    void closePort();
    bool isOpen() const;

    ssize_t readBytes(uint8_t* data, size_t len, int timeout_ms, std::string* error);
    bool writeAll(const uint8_t* data, size_t len, std::string* error);

private:
    int fd_;
};

#endif

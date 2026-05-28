#include "uart_driver.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace {

speed_t baudrateToTermios(int baudrate)
{
    switch (baudrate) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
#ifdef B460800
    case 460800:
        return B460800;
#endif
#ifdef B921600
    case 921600:
        return B921600;
#endif
    default:
        return 0;
    }
}

void setError(std::string* error, const std::string& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

UartDriver::UartDriver()
    : fd_(-1)
{
}

UartDriver::~UartDriver()
{
    closePort();
}

bool UartDriver::openPort(const std::string& device, int baudrate, std::string* error)
{
    closePort();

    const speed_t speed = baudrateToTermios(baudrate);
    if (speed == 0) {
        setError(error, "unsupported baudrate: " + std::to_string(baudrate));
        return false;
    }

    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        setError(error, "open failed for " + device + ": " + std::strerror(errno));
        return false;
    }

    termios tty {};
    if (tcgetattr(fd_, &tty) != 0) {
        setError(error, "tcgetattr failed: " + std::string(std::strerror(errno)));
        closePort();
        return false;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    tty.c_cflag &= static_cast<tcflag_t>(~PARENB);
    tty.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
    tty.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    tty.c_cflag |= CS8;
    tty.c_cflag &= static_cast<tcflag_t>(~CRTSCTS);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        setError(error, "tcsetattr failed: " + std::string(std::strerror(errno)));
        closePort();
        return false;
    }

    tcflush(fd_, TCIOFLUSH);
    return true;
}

void UartDriver::closePort()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool UartDriver::isOpen() const
{
    return fd_ >= 0;
}

ssize_t UartDriver::readBytes(uint8_t* data, size_t len, int timeout_ms, std::string* error)
{
    if (fd_ < 0) {
        setError(error, "read requested before UART port was opened");
        return -1;
    }
    if (data == nullptr || len == 0u) {
        return 0;
    }

    pollfd pfd {};
    pfd.fd = fd_;
    pfd.events = POLLIN;

    const int ready = poll(&pfd, 1, timeout_ms);
    if (ready < 0) {
        if (errno == EINTR) {
            return 0;
        }
        setError(error, "poll failed: " + std::string(std::strerror(errno)));
        return -1;
    }
    if (ready == 0) {
        return 0;
    }

    const ssize_t count = ::read(fd_, data, len);
    if (count < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        setError(error, "read failed: " + std::string(std::strerror(errno)));
        return -1;
    }

    return count;
}

bool UartDriver::writeAll(const uint8_t* data, size_t len, std::string* error)
{
    if (fd_ < 0) {
        setError(error, "write requested before UART port was opened");
        return false;
    }
    if (data == nullptr && len != 0u) {
        setError(error, "null write buffer");
        return false;
    }

    size_t written = 0u;
    while (written < len) {
        const ssize_t count = ::write(fd_, &data[written], len - written);
        if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                pollfd pfd {};
                pfd.fd = fd_;
                pfd.events = POLLOUT;
                if (poll(&pfd, 1, 1000) <= 0) {
                    setError(error, "timed out waiting for UART transmit space");
                    return false;
                }
                continue;
            }
            setError(error, "write failed: " + std::string(std::strerror(errno)));
            return false;
        }
        written += static_cast<size_t>(count);
    }

    if (tcdrain(fd_) != 0) {
        setError(error, "tcdrain failed: " + std::string(std::strerror(errno)));
        return false;
    }

    return true;
}

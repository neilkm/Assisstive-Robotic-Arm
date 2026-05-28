#pragma once

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>
#include <cstdint>

// Bidirectional Unix socketpair that mimics a serial link.
// jetsonFd() is the "Jetson" end; deviceFd() is the "MCU" end.
class VirtualSerialPair {
public:
    VirtualSerialPair() { recreate(); }
    ~VirtualSerialPair() { closeFd(0); closeFd(1); }

    VirtualSerialPair(const VirtualSerialPair&) = delete;
    VirtualSerialPair& operator=(const VirtualSerialPair&) = delete;

    int jetsonFd() const { return fds_[0]; }
    int deviceFd() const { return fds_[1]; }

    // Simulate MCU power cycle: close both ends and reopen.
    void recreate()
    {
        closeFd(0); closeFd(1);
        int sv[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
            throw std::runtime_error(std::string("socketpair: ") + std::strerror(errno));
        }
        fds_[0] = sv[0];
        fds_[1] = sv[1];
    }

    // Simulate MCU side disconnect (leaves Jetson end open so reads return 0).
    void closeDeviceFd() { closeFd(1); }
    void closeJetsonFd() { closeFd(0); }

private:
    void closeFd(int idx)
    {
        if (fds_[idx] >= 0) {
            ::close(fds_[idx]);
            fds_[idx] = -1;
        }
    }

    int fds_[2] = {-1, -1};
};

// Write all bytes to fd; returns false if a write error occurs.
inline bool fd_write_all(int fd, const uint8_t* data, size_t len)
{
    size_t written = 0;
    while (written < len) {
        const ssize_t n = ::write(fd, data + written, len - written);
        if (n <= 0) return false;
        written += static_cast<size_t>(n);
    }
    return true;
}

// Read bytes with a timeout. Returns >0 on data, 0 on timeout, -1 on error.
inline ssize_t fd_read_timeout(int fd, uint8_t* buf, size_t len, int timeout_ms)
{
    struct pollfd pfd {};
    pfd.fd = fd;
    pfd.events = POLLIN;
    const int r = ::poll(&pfd, 1, timeout_ms);
    if (r <= 0) return static_cast<ssize_t>(r);
    return ::read(fd, buf, len);
}

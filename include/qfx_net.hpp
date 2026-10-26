#pragma once

#include "qfx.hpp"
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

namespace qfx {

class LowLatencySocket {
public:
    int fd = -1;

    ~LowLatencySocket() {
        close_socket();
    }

    bool open_socket() noexcept {
        close_socket();
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return false;
        return apply_low_latency_options();
    }

    bool apply_low_latency_options() noexcept {
        if (fd < 0) return false;

        int flag = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) != 0) {
            return false;
        }

#if defined(SO_BUSY_POLL)
        int busy_poll_us = 50;
        setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll_us, sizeof(busy_poll_us));
#endif

        int buf_size = 262144;
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
        return true;
    }

    bool bind_and_listen(uint16_t port) noexcept {
        if (fd < 0 && !open_socket()) return false;

        int reuse = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);

        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            return false;
        }
        return listen(fd, 32) == 0;
    }

    bool connect_to(const char* ip, uint16_t port) noexcept {
        if (fd < 0 && !open_socket()) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
            return false;
        }
        int rc = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        return rc == 0 || errno == EINPROGRESS;
    }

    ssize_t send_bytes(const uint8_t* data, size_t len) noexcept {
        if (fd < 0) return -1;
        return send(fd, data, len, MSG_NOSIGNAL);
    }

    ssize_t recv_bytes(uint8_t* buf, size_t max_len) noexcept {
        if (fd < 0) return -1;
        return recv(fd, buf, max_len, 0);
    }

    void close_socket() noexcept {
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
    }
};

}

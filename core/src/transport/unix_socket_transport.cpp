#include "ibridger/transport/unix_socket_transport.h"

#if defined(__unix__) || defined(__APPLE__)

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace ibridger {
namespace transport {

// ─── UnixSocketConnection ─────────────────────────────────────────────────────

UnixSocketConnection::UnixSocketConnection(int fd, ConnectionId id)
    : fd_(fd), id_(id), connected_(true) {}

UnixSocketConnection::~UnixSocketConnection() {
    close();
}

std::pair<size_t, std::error_code> UnixSocketConnection::send(
    const uint8_t* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::write(fd_, data + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            connected_ = false;
            return {total, std::error_code(errno, std::system_category())};
        }
        if (n == 0) break;
        total += static_cast<size_t>(n);
    }
    return {total, {}};
}

std::pair<size_t, std::error_code> UnixSocketConnection::recv(
    uint8_t* buf, size_t len) {
    while (true) {
        ssize_t n = ::read(fd_, buf, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            connected_ = false;
            return {0, std::error_code(errno, std::system_category())};
        }
        if (n == 0) {
            // Clean EOF — peer closed the connection.
            connected_ = false;
            return {0, {}};
        }
        return {static_cast<size_t>(n), {}};
    }
}

void UnixSocketConnection::close() {
    if (connected_.exchange(false)) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool UnixSocketConnection::is_connected() const {
    return connected_;
}

ConnectionId UnixSocketConnection::id() const {
    return id_;
}

// ─── UnixSocketTransport ──────────────────────────────────────────────────────

UnixSocketTransport::UnixSocketTransport()
    : server_fd_(-1), next_id_(1) {}

UnixSocketTransport::~UnixSocketTransport() {
    close();
}

std::error_code UnixSocketTransport::listen(const std::string& endpoint) {
    server_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        return std::error_code(errno, std::system_category());
    }

    // Remove a stale socket file from a previous run.
    ::unlink(endpoint.c_str());

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, endpoint.c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        int err = errno;
        ::close(server_fd_);
        server_fd_ = -1;
        return std::error_code(err, std::system_category());
    }

    if (::listen(server_fd_, SOMAXCONN) < 0) {
        int err = errno;
        ::close(server_fd_);
        server_fd_ = -1;
        return std::error_code(err, std::system_category());
    }

    endpoint_ = endpoint;
    return {};
}

std::pair<std::unique_ptr<IConnection>, std::error_code>
UnixSocketTransport::accept() {
    int client_fd = ::accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
        return std::make_pair(std::unique_ptr<IConnection>(),
                              std::error_code(errno, std::system_category()));
    }
    std::unique_ptr<IConnection> conn =
        std::make_unique<UnixSocketConnection>(client_fd, next_id_++);
    return std::make_pair(std::move(conn), std::error_code{});
}

std::pair<std::unique_ptr<IConnection>, std::error_code>
UnixSocketTransport::connect(const std::string& endpoint) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return std::make_pair(std::unique_ptr<IConnection>(),
                              std::error_code(errno, std::system_category()));
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, endpoint.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        int err = errno;
        ::close(fd);
        return std::make_pair(std::unique_ptr<IConnection>(),
                              std::error_code(err, std::system_category()));
    }

    std::unique_ptr<IConnection> conn =
        std::make_unique<UnixSocketConnection>(fd, next_id_++);
    return std::make_pair(std::move(conn), std::error_code{});
}

void UnixSocketTransport::close() {
    if (server_fd_ >= 0) {
        ::close(server_fd_);
        server_fd_ = -1;
    }
    if (!endpoint_.empty()) {
        ::unlink(endpoint_.c_str());
        endpoint_.clear();
    }
}

} // namespace transport
} // namespace ibridger

#endif // defined(__unix__) || defined(__APPLE__)

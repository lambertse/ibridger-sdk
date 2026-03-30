#pragma once

#include "ibridger/transport/transport.h"
#include "ibridger/transport/connection.h"

#include <atomic>
#include <string>

#if defined(__unix__) || defined(__APPLE__)

namespace ibridger {
namespace transport {

/// A connected Unix Domain Socket endpoint.
class UnixSocketConnection : public IConnection {
public:
    UnixSocketConnection(int fd, ConnectionId id);
    ~UnixSocketConnection() override;

    /// Sends exactly `len` bytes, looping over partial writes. Returns (len, {})
    /// on success or (bytes_sent, error) on failure.
    std::pair<size_t, std::error_code> send(const uint8_t* data, size_t len) override;

    /// Receives up to `len` bytes (single syscall). Returns (0, {}) on clean EOF.
    std::pair<size_t, std::error_code> recv(uint8_t* buf, size_t len) override;

    void close() override;
    bool is_connected() const override;
    ConnectionId id() const override;

private:
    int fd_;
    ConnectionId id_;
    std::atomic<bool> connected_;
};

/// Unix Domain Socket transport (AF_UNIX, SOCK_STREAM).
///
/// Server usage:  listen() → accept() loop
/// Client usage:  connect()
class UnixSocketTransport : public ITransport {
public:
    UnixSocketTransport();
    ~UnixSocketTransport() override;

    std::error_code listen(const std::string& endpoint) override;
    std::pair<std::unique_ptr<IConnection>, std::error_code> accept() override;
    std::pair<std::unique_ptr<IConnection>, std::error_code> connect(const std::string& endpoint) override;
    void close() override;

private:
    int server_fd_;
    std::string endpoint_;
    std::atomic<ConnectionId> next_id_;
};

} // namespace transport
} // namespace ibridger

#endif // defined(__unix__) || defined(__APPLE__)

#pragma once

#include "ibridger/transport/transport.h"
#include "ibridger/transport/connection.h"

#include <atomic>
#include <string>

namespace ibridger {
namespace transport {

#ifdef _WIN32

/// A connected Named Pipe endpoint (Windows).
class NamedPipeConnection : public IConnection {
public:
    // TODO: accept a HANDLE and an id
    explicit NamedPipeConnection(void* handle, ConnectionId id);
    ~NamedPipeConnection() override;

    std::pair<size_t, std::error_code> send(const uint8_t* data, size_t len) override;
    std::pair<size_t, std::error_code> recv(uint8_t* buf, size_t len) override;
    void close() override;
    bool is_connected() const override;
    ConnectionId id() const override;

private:
    void* handle_;  // HANDLE
    ConnectionId id_;
    std::atomic<bool> connected_;
};

/// Named Pipe transport (Windows).
///
/// Server usage:  listen() → accept() loop
/// Client usage:  connect()
class NamedPipeTransport : public ITransport {
public:
    NamedPipeTransport();
    ~NamedPipeTransport() override;

    std::error_code listen(const std::string& endpoint) override;
    std::pair<std::unique_ptr<IConnection>, std::error_code> accept() override;
    std::pair<std::unique_ptr<IConnection>, std::error_code> connect(const std::string& endpoint) override;
    void close() override;

private:
    void* server_handle_;  // HANDLE
    std::string endpoint_;
    std::atomic<ConnectionId> next_id_;
};

#else // non-Windows

/// Named Pipe transport — stub for non-Windows platforms.
/// All methods return std::errc::not_supported.
class NamedPipeConnection : public IConnection {
public:
    NamedPipeConnection() = default;
    ~NamedPipeConnection() override = default;

    std::pair<size_t, std::error_code> send(const uint8_t*, size_t) override {
        return {0, std::make_error_code(std::errc::not_supported)};
    }
    std::pair<size_t, std::error_code> recv(uint8_t*, size_t) override {
        return {0, std::make_error_code(std::errc::not_supported)};
    }
    void close() override {}
    bool is_connected() const override { return false; }
    ConnectionId id() const override { return 0; }
};

/// Named Pipe transport — stub for non-Windows platforms.
class NamedPipeTransport : public ITransport {
public:
    NamedPipeTransport() = default;
    ~NamedPipeTransport() override = default;

    std::error_code listen(const std::string&) override {
        return std::make_error_code(std::errc::not_supported);
    }
    std::pair<std::unique_ptr<IConnection>, std::error_code> accept() override {
        return {nullptr, std::make_error_code(std::errc::not_supported)};
    }
    std::pair<std::unique_ptr<IConnection>, std::error_code> connect(const std::string&) override {
        return {nullptr, std::make_error_code(std::errc::not_supported)};
    }
    void close() override {}
};

#endif // _WIN32

} // namespace transport
} // namespace ibridger

#include "ibridger/transport/named_pipe_transport.h"

#ifdef _WIN32

#include <windows.h>

namespace ibridger {
namespace transport {

// ─── NamedPipeConnection ──────────────────────────────────────────────────────

NamedPipeConnection::NamedPipeConnection(void* handle, ConnectionId id)
    : handle_(handle), id_(id), connected_(true) {}

NamedPipeConnection::~NamedPipeConnection() {
    close();
}

std::pair<size_t, std::error_code> NamedPipeConnection::send(
    const uint8_t* data, size_t len) {
    // TODO: implement Windows named pipe write
    // Loop using WriteFile(handle_, ...) until all `len` bytes are written.
    // Handle partial writes and ERROR_IO_PENDING for overlapped I/O.
    (void)data;
    (void)len;
    return {0, std::make_error_code(std::errc::not_supported)};
}

std::pair<size_t, std::error_code> NamedPipeConnection::recv(
    uint8_t* buf, size_t len) {
    // TODO: implement Windows named pipe read
    // Use ReadFile(handle_, buf, len, &bytes_read, nullptr).
    // Return (0, {}) on broken pipe (ERROR_BROKEN_PIPE / ERROR_PIPE_NOT_CONNECTED).
    (void)buf;
    (void)len;
    return {0, std::make_error_code(std::errc::not_supported)};
}

void NamedPipeConnection::close() {
    // TODO: implement Windows named pipe close
    // if (connected_.exchange(false)) { CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE; }
}

bool NamedPipeConnection::is_connected() const {
    return connected_;
}

ConnectionId NamedPipeConnection::id() const {
    return id_;
}

// ─── NamedPipeTransport ───────────────────────────────────────────────────────

NamedPipeTransport::NamedPipeTransport()
    : server_handle_(INVALID_HANDLE_VALUE), next_id_(1) {}

NamedPipeTransport::~NamedPipeTransport() {
    close();
}

std::error_code NamedPipeTransport::listen(const std::string& endpoint) {
    // TODO: implement Windows named pipe server setup
    // 1. Validate / normalise endpoint to \\.\pipe\<name> format.
    // 2. CreateNamedPipe(endpoint, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
    //                   PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, nullptr)
    // 3. Store handle in server_handle_ and endpoint_ on success.
    (void)endpoint;
    return std::make_error_code(std::errc::not_supported);
}

std::pair<std::unique_ptr<IConnection>, std::error_code>
NamedPipeTransport::accept() {
    // TODO: implement Windows named pipe accept
    // 1. ConnectNamedPipe(server_handle_, nullptr) — blocks until a client connects.
    // 2. On success, wrap server_handle_ in NamedPipeConnection and create a fresh
    //    server_handle_ for the next accept().
    return std::make_pair(std::unique_ptr<IConnection>(),
                          std::make_error_code(std::errc::not_supported));
}

std::pair<std::unique_ptr<IConnection>, std::error_code>
NamedPipeTransport::connect(const std::string& endpoint) {
    // TODO: implement Windows named pipe client connect
    // 1. WaitNamedPipe(endpoint, NMPWAIT_USE_DEFAULT_WAIT) if pipe busy.
    // 2. CreateFile(endpoint, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
    //               OPEN_EXISTING, 0, nullptr)
    // 3. Wrap resulting HANDLE in NamedPipeConnection.
    (void)endpoint;
    return std::make_pair(std::unique_ptr<IConnection>(),
                          std::make_error_code(std::errc::not_supported));
}

void NamedPipeTransport::close() {
    // TODO: implement Windows named pipe server close
    // if (server_handle_ != INVALID_HANDLE_VALUE) {
    //     DisconnectNamedPipe(server_handle_);
    //     CloseHandle(server_handle_);
    //     server_handle_ = INVALID_HANDLE_VALUE;
    // }
}

} // namespace transport
} // namespace ibridger

#endif // _WIN32

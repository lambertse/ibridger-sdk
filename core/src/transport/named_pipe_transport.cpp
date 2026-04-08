#ifdef _WIN32
#include "ibridger/transport/named_pipe_transport.h"

#include <windows.h>

namespace ibridger {
namespace transport {

// ─── Helpers
// ──────────────────────────────────────────────────────────────────

static std::error_code win_error(DWORD code = ::GetLastError()) {
  return {static_cast<int>(code), std::system_category()};
}

/// Normalise an endpoint string to the \\.\pipe\<name> form.
/// Accepts either the full UNC path or a bare pipe name.
static std::string normalise_pipe_name(const std::string& endpoint) {
  const std::string prefix = "\\\\.\\pipe\\";
  if (endpoint.rfind(prefix, 0) == 0) return endpoint;
  return prefix + endpoint;
}

// ─── NamedPipeConnection ───────────────────────────────────────────────────

NamedPipeConnection::NamedPipeConnection()
    : handle_(nullptr), id_(0), connected_(false) {}

NamedPipeConnection::NamedPipeConnection(void* handle, ConnectionId id)
    : handle_(handle), id_(id), connected_(true) {}

NamedPipeConnection::~NamedPipeConnection() { close(); }

std::pair<size_t, std::error_code> NamedPipeConnection::send(
    const uint8_t* data, size_t len) {
  if (!handle_) {
    return {0, std::make_error_code(std::errc::not_supported)};
  }
  HANDLE h = reinterpret_cast<HANDLE>(handle_);
  size_t total = 0;
  while (total < len) {
    DWORD written = 0;
    BOOL ok = ::WriteFile(h, data + total, static_cast<DWORD>(len - total),
                          &written, nullptr);
    if (!ok) {
      DWORD err = ::GetLastError();
      connected_ = false;
      return {total, win_error(err)};
    }
    total += static_cast<size_t>(written);
  }
  return {total, {}};
}

std::pair<size_t, std::error_code> NamedPipeConnection::recv(uint8_t* buf,
                                                             size_t len) {
  if (!handle_) {
    return {0, std::make_error_code(std::errc::not_supported)};
  }
  HANDLE h = reinterpret_cast<HANDLE>(handle_);
  DWORD read = 0;
  BOOL ok = ::ReadFile(h, buf, static_cast<DWORD>(len), &read, nullptr);
  if (!ok) {
    DWORD err = ::GetLastError();
    if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
      // Clean EOF — peer closed the connection.
      connected_ = false;
      return {0, {}};
    }
    connected_ = false;
    return {0, win_error(err)};
  }
  if (read == 0) {
    // EOF with no error (shouldn't normally happen with byte-mode pipes,
    // but handle gracefully).
    connected_ = false;
    return {0, {}};
  }
  return {static_cast<size_t>(read), {}};
}

void NamedPipeConnection::close() {
  if (connected_.exchange(false)) {
    ::CloseHandle(reinterpret_cast<HANDLE>(handle_));
    handle_ = nullptr;
  }
}

bool NamedPipeConnection::is_connected() const { return connected_; }

ConnectionId NamedPipeConnection::id() const { return id_; }

// ─── NamedPipeTransport ────────────────────────────────────────────────────

NamedPipeTransport::NamedPipeTransport()
    : server_handle_(INVALID_HANDLE_VALUE), next_id_(1) {}

NamedPipeTransport::~NamedPipeTransport() { close(); }

/// Create a new server-side pipe instance at the given (already normalised)
/// endpoint and store it in server_handle_.
static std::error_code create_pipe_instance(const std::string& endpoint,
                                            void** out_handle) {
  HANDLE h = ::CreateNamedPipeA(endpoint.c_str(), PIPE_ACCESS_DUPLEX,
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                PIPE_UNLIMITED_INSTANCES, 65536, 65536,
                                0,       // default timeout (50 ms)
                                nullptr  // default security attributes
  );
  if (h == INVALID_HANDLE_VALUE) return win_error();
  *out_handle = h;
  return {};
}

std::error_code NamedPipeTransport::listen(const std::string& endpoint) {
  endpoint_ = normalise_pipe_name(endpoint);
  void* h = nullptr;
  auto err = create_pipe_instance(endpoint_, &h);
  if (err) return err;
  server_handle_ = h;
  return {};
}

std::pair<std::unique_ptr<IConnection>, std::error_code>
NamedPipeTransport::accept() {
  HANDLE h = reinterpret_cast<HANDLE>(server_handle_);

  // Block until a client connects.
  BOOL ok = ::ConnectNamedPipe(h, nullptr);
  if (!ok) {
    DWORD err = ::GetLastError();
    if (err != ERROR_PIPE_CONNECTED) {
      // ERROR_PIPE_CONNECTED means the client connected before we called
      // ConnectNamedPipe — that's fine. Any other error is real.
      return {nullptr, win_error(err)};
    }
  }

  // Hand the current handle to the new connection.
  ConnectionId id = next_id_++;
  std::unique_ptr<IConnection> conn =
      std::make_unique<NamedPipeConnection>(server_handle_, id);

  // Create a fresh pipe instance for the next accept() call.
  void* next_h = nullptr;
  auto create_err = create_pipe_instance(endpoint_, &next_h);
  if (create_err) {
    server_handle_ = INVALID_HANDLE_VALUE;
    return std::make_pair(std::move(conn), std::error_code{});
  }
  server_handle_ = next_h;

  return std::make_pair(std::move(conn), std::error_code{});
}

std::pair<std::unique_ptr<IConnection>, std::error_code>
NamedPipeTransport::connect(const std::string& endpoint) {
  std::string name = normalise_pipe_name(endpoint);

  // If the pipe is busy, wait for an instance to become available.
  if (!::WaitNamedPipeA(name.c_str(), NMPWAIT_USE_DEFAULT_WAIT)) {
    DWORD err = ::GetLastError();
    if (err != ERROR_FILE_NOT_FOUND) {
      // Pipe exists but timed out — treat as transient error.
      return {nullptr, win_error(err)};
    }
    // ERROR_FILE_NOT_FOUND: pipe doesn't exist yet; fall through to CreateFile
    // which will produce a cleaner "file not found" error.
  }

  HANDLE h = ::CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                           0,        // no sharing
                           nullptr,  // default security
                           OPEN_EXISTING,
                           0,       // synchronous I/O
                           nullptr  // no template
  );
  if (h == INVALID_HANDLE_VALUE) {
    return {nullptr, win_error()};
  }

  std::unique_ptr<IConnection> conn =
      std::make_unique<NamedPipeConnection>(h, next_id_++);
  return std::make_pair(std::move(conn), std::error_code{});
}

void NamedPipeTransport::close() {
  if (server_handle_ != INVALID_HANDLE_VALUE) {
    ::DisconnectNamedPipe(reinterpret_cast<HANDLE>(server_handle_));
    ::CloseHandle(reinterpret_cast<HANDLE>(server_handle_));
    server_handle_ = INVALID_HANDLE_VALUE;
  }
  endpoint_.clear();
}

}  // namespace transport
}  // namespace ibridger

#else  // !_WIN32 ─── stubs so the translation unit links on Unix/macOS ────────

#include "ibridger/transport/named_pipe_transport.h"

namespace ibridger {
namespace transport {

static const auto kNotSupported =
    std::make_error_code(std::errc::not_supported);

NamedPipeConnection::NamedPipeConnection()
    : handle_(nullptr), id_(0), connected_(false) {}

NamedPipeConnection::NamedPipeConnection(void* handle, ConnectionId id)
    : handle_(handle), id_(id), connected_(false) {}

NamedPipeConnection::~NamedPipeConnection() {}

std::pair<size_t, std::error_code> NamedPipeConnection::send(const uint8_t*,
                                                             size_t) {
  return {0, kNotSupported};
}

std::pair<size_t, std::error_code> NamedPipeConnection::recv(uint8_t*, size_t) {
  return {0, kNotSupported};
}

void NamedPipeConnection::close() {}

bool NamedPipeConnection::is_connected() const { return connected_; }

ConnectionId NamedPipeConnection::id() const { return id_; }

NamedPipeTransport::NamedPipeTransport()
    : server_handle_(nullptr), next_id_(1) {}

NamedPipeTransport::~NamedPipeTransport() {}

std::error_code NamedPipeTransport::listen(const std::string&) {
  return kNotSupported;
}

std::pair<std::unique_ptr<IConnection>, std::error_code>
NamedPipeTransport::accept() {
  return {nullptr, kNotSupported};
}

std::pair<std::unique_ptr<IConnection>, std::error_code>
NamedPipeTransport::connect(const std::string&) {
  return {nullptr, kNotSupported};
}

void NamedPipeTransport::close() {}

}  // namespace transport
}  // namespace ibridger

#endif  // _WIN32

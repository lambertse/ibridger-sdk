#pragma once

// Platform-native connected transport pair utilities for tests.
//
// Provides:
//   make_endpoint()         — unique endpoint string for the current platform
//   cleanup_endpoint()      — remove socket file (Unix only; no-op on Windows)
//   make_framed_pair()      — two connected FramedConnections
//   make_codec_pair()       — two connected EnvelopeCodecs
//
// Unix/macOS: uses socketpair() — no threads required.
// Windows:    uses NamedPipeTransport with a server thread.

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "ibridger/protocol/envelope_codec.h"
#include "ibridger/protocol/framing.h"
#include "ibridger/transport/connection.h"

#if defined(__unix__) || defined(__APPLE__)
#include <sys/socket.h>
#include <unistd.h>

#include "ibridger/transport/unix_socket_transport.h"
#elif defined(_WIN32)
#include "ibridger/transport/named_pipe_transport.h"
#endif

namespace ibridger {
namespace test {

/// Returns a unique endpoint string with no server listening on it.
/// Unix/macOS: a temp file path (file removed before returning).
/// Windows:    a named pipe path with a per-process counter suffix.
inline std::string make_endpoint() {
#if defined(__unix__) || defined(__APPLE__)
  char tpl[] = "/tmp/ibridger_test_XXXXXX";
  int fd = ::mkstemp(tpl);
  ::close(fd);
  ::unlink(tpl);
  return tpl;
#elif defined(_WIN32)
  static std::atomic<int> counter{0};
  return "\\\\.\\pipe\\ibridger_test_" +
         std::to_string(static_cast<unsigned long>(::GetCurrentProcessId())) +
         "_" + std::to_string(counter++);
#endif
}

/// Remove the socket file after a test.  No-op on Windows.
inline void cleanup_endpoint(const std::string& endpoint) {
#if defined(__unix__) || defined(__APPLE__)
  ::unlink(endpoint.c_str());
#else
  (void)endpoint;
#endif
}

namespace detail {

/// Build a connected pair of raw IConnection unique_ptrs.
inline std::pair<std::unique_ptr<transport::IConnection>,
                 std::unique_ptr<transport::IConnection>>
make_connection_pair() {
#if defined(__unix__) || defined(__APPLE__)
  int fds[2];
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
    throw std::runtime_error("socketpair failed");
  return {
      std::make_unique<transport::UnixSocketConnection>(fds[0], 1),
      std::make_unique<transport::UnixSocketConnection>(fds[1], 2),
  };
#elif defined(_WIN32)
  std::string name = make_endpoint();

  transport::NamedPipeTransport server_transport;
  if (auto err = server_transport.listen(name); err)
    throw std::runtime_error("listen failed: " + err.message());

  std::unique_ptr<transport::IConnection> server_conn;
  std::exception_ptr accept_ex;

  std::thread t([&] {
    try {
      auto [conn, err] = server_transport.accept();
      if (err) throw std::runtime_error("accept failed: " + err.message());
      server_conn = std::move(conn);
    } catch (...) {
      accept_ex = std::current_exception();
    }
  });

  transport::NamedPipeTransport client_transport;
  auto [client_conn, cerr] = client_transport.connect(name);
  t.join();

  if (accept_ex) std::rethrow_exception(accept_ex);
  if (cerr) throw std::runtime_error("connect failed: " + cerr.message());
  if (!server_conn) throw std::runtime_error("accept returned null connection");

  return {std::move(server_conn), std::move(client_conn)};
#endif
}

}  // namespace detail

/// Returns two connected FramedConnections backed by the platform-native
/// transport.
inline std::pair<protocol::FramedConnection, protocol::FramedConnection>
make_framed_pair() {
  auto [a, b] = detail::make_connection_pair();
  return {protocol::FramedConnection(std::move(a)),
          protocol::FramedConnection(std::move(b))};
}

/// Returns two connected EnvelopeCodecs backed by the platform-native
/// transport.
inline std::pair<protocol::EnvelopeCodec, protocol::EnvelopeCodec>
make_codec_pair() {
  auto [a, b] = detail::make_connection_pair();
  auto fa = std::make_shared<protocol::FramedConnection>(std::move(a));
  auto fb = std::make_shared<protocol::FramedConnection>(std::move(b));
  return {protocol::EnvelopeCodec(fa), protocol::EnvelopeCodec(fb)};
}

}  // namespace test
}  // namespace ibridger

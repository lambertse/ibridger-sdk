#include <gtest/gtest.h>

#include "ibridger/transport/named_pipe_transport.h"

#ifdef _WIN32

namespace {

std::string make_pipe_name() {
  // Use the current PID so parallel test runs don't collide.
  return "\\\\.\\pipe\\ibridger_test_" +
         std::to_string(static_cast<unsigned long>(::GetCurrentProcessId()));
}

}  // namespace

// ─── Windows: full test suite mirroring unix_socket_test.cpp ─────────────────

class NamedPipeTest : public ::testing::Test {
 protected:
  std::string pipe_name;

  void SetUp() override { pipe_name = make_pipe_name(); }
  // Named pipes don't leave filesystem artifacts — no TearDown needed.
};

// ─── Connect / accept handshake
// ───────────────────────────────────────────────

TEST_F(NamedPipeTest, ConnectAcceptHandshake) {
  using namespace ibridger::transport;

  NamedPipeTransport server;
  ASSERT_FALSE(server.listen(pipe_name));

  std::thread server_thread([&]() {
    auto [conn, err] = server.accept();
    EXPECT_FALSE(err) << err.message();
    ASSERT_NE(conn, nullptr);
    EXPECT_TRUE(conn->is_connected());
  });

  NamedPipeTransport client_transport;
  auto [conn, err] = client_transport.connect(pipe_name);
  EXPECT_FALSE(err) << err.message();
  ASSERT_NE(conn, nullptr);
  EXPECT_TRUE(conn->is_connected());

  server_thread.join();
}

// ─── Small payload roundtrip
// ──────────────────────────────────────────────────

TEST_F(NamedPipeTest, SendRecvSmallPayload) {
  using namespace ibridger::transport;

  NamedPipeTransport server;
  ASSERT_FALSE(server.listen(pipe_name));

  const std::string message = "hello ibridger";
  std::string received;

  std::thread server_thread([&]() {
    auto [conn, err] = server.accept();
    ASSERT_FALSE(err);

    uint8_t buf[256] = {};
    auto [n, recv_err] = conn->recv(buf, sizeof(buf));
    EXPECT_FALSE(recv_err) << recv_err.message();
    received = std::string(reinterpret_cast<char*>(buf), n);
  });

  NamedPipeTransport client_transport;
  auto [conn, err] = client_transport.connect(pipe_name);
  ASSERT_FALSE(err);

  auto [sent, send_err] = conn->send(
      reinterpret_cast<const uint8_t*>(message.data()), message.size());
  EXPECT_FALSE(send_err) << send_err.message();
  EXPECT_EQ(sent, message.size());

  server_thread.join();
  EXPECT_EQ(received, message);
}

// ─── 1 MB payload (exercises partial write / read loops) ─────────────────────

TEST_F(NamedPipeTest, SendRecvLargePayload) {
  using namespace ibridger::transport;

  NamedPipeTransport server;
  ASSERT_FALSE(server.listen(pipe_name));

  constexpr size_t kSize = 1024 * 1024;  // 1 MB
  std::vector<uint8_t> payload(kSize, 0xAB);
  std::vector<uint8_t> received(kSize, 0x00);

  std::thread server_thread([&]() {
    auto [conn, err] = server.accept();
    ASSERT_FALSE(err);

    size_t total = 0;
    while (total < kSize) {
      auto [n, recv_err] = conn->recv(received.data() + total, kSize - total);
      if (n == 0) break;
      ASSERT_FALSE(recv_err) << recv_err.message();
      total += n;
    }
    EXPECT_EQ(total, kSize);
  });

  NamedPipeTransport client_transport;
  auto [conn, err] = client_transport.connect(pipe_name);
  ASSERT_FALSE(err);

  auto [sent, send_err] = conn->send(payload.data(), kSize);
  EXPECT_FALSE(send_err) << send_err.message();
  EXPECT_EQ(sent, kSize);
  conn->close();

  server_thread.join();
  EXPECT_EQ(payload, received);
}

// ─── Close detection (recv returns 0 on clean EOF) ───────────────────────────

TEST_F(NamedPipeTest, CloseDetection) {
  using namespace ibridger::transport;

  NamedPipeTransport server;
  ASSERT_FALSE(server.listen(pipe_name));

  std::thread server_thread([&]() {
    auto [conn, err] = server.accept();
    ASSERT_FALSE(err);
    // Server immediately closes its end.
    conn->close();
    EXPECT_FALSE(conn->is_connected());
  });

  NamedPipeTransport client_transport;
  auto [conn, err] = client_transport.connect(pipe_name);
  ASSERT_FALSE(err);

  server_thread.join();

  // After the server closes, recv must return 0 with no error (clean EOF).
  uint8_t buf[16];
  auto [n, recv_err] = conn->recv(buf, sizeof(buf));
  EXPECT_EQ(n, 0u);
  EXPECT_FALSE(recv_err) << recv_err.message();
}

// ─── Multiple sequential connections ─────────────────────────────────────────

TEST_F(NamedPipeTest, MultipleSequentialConnections) {
  using namespace ibridger::transport;

  NamedPipeTransport server;
  ASSERT_FALSE(server.listen(pipe_name));

  constexpr int kConnections = 3;
  std::vector<ConnectionId> server_ids;

  std::thread server_thread([&]() {
    for (int i = 0; i < kConnections; i++) {
      auto [conn, err] = server.accept();
      EXPECT_FALSE(err) << err.message();
      ASSERT_NE(conn, nullptr);
      server_ids.push_back(conn->id());
    }
  });

  std::vector<ConnectionId> client_ids;
  for (int i = 0; i < kConnections; i++) {
    NamedPipeTransport client_transport;
    auto [conn, err] = client_transport.connect(pipe_name);
    EXPECT_FALSE(err) << err.message();
    ASSERT_NE(conn, nullptr);
    client_ids.push_back(conn->id());
  }

  server_thread.join();

  EXPECT_EQ(static_cast<int>(server_ids.size()), kConnections);
  EXPECT_EQ(static_cast<int>(client_ids.size()), kConnections);
}

#else  // !_WIN32 ─── verify all operations return not_supported ───────────────

TEST(NamedPipeTest, ListenReturnsNotSupported) {
  ibridger::transport::NamedPipeTransport transport;
  auto err = transport.listen("/tmp/ibridger_named_pipe_test");
  EXPECT_EQ(err, std::make_error_code(std::errc::not_supported));
}

TEST(NamedPipeTest, ConnectReturnsNotSupported) {
  ibridger::transport::NamedPipeTransport transport;
  auto [conn, err] = transport.connect("/tmp/ibridger_named_pipe_test");
  EXPECT_EQ(err, std::make_error_code(std::errc::not_supported));
  EXPECT_EQ(conn, nullptr);
}

TEST(NamedPipeTest, AcceptReturnsNotSupported) {
  ibridger::transport::NamedPipeTransport transport;
  auto [conn, err] = transport.accept();
  EXPECT_EQ(err, std::make_error_code(std::errc::not_supported));
  EXPECT_EQ(conn, nullptr);
}

TEST(NamedPipeTest, ConnectionSendReturnsNotSupported) {
  ibridger::transport::NamedPipeConnection conn;
  uint8_t buf[4] = {1, 2, 3, 4};
  auto [n, err] = conn.send(buf, sizeof(buf));
  EXPECT_EQ(n, 0u);
  EXPECT_EQ(err, std::make_error_code(std::errc::not_supported));
}

TEST(NamedPipeTest, ConnectionRecvReturnsNotSupported) {
  ibridger::transport::NamedPipeConnection conn;
  uint8_t buf[4] = {};
  auto [n, err] = conn.recv(buf, sizeof(buf));
  EXPECT_EQ(n, 0u);
  EXPECT_EQ(err, std::make_error_code(std::errc::not_supported));
}

#endif  // _WIN32

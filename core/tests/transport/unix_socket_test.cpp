#include <gtest/gtest.h>

#if defined(__unix__) || defined(__APPLE__)

#include "ibridger/transport/unix_socket_transport.h"

#include <thread>
#include <string>
#include <vector>
#include <unistd.h>

namespace {

// Returns a unique temp path suitable for a Unix socket.
std::string make_socket_path() {
    char tpl[] = "/tmp/ibridger_test_XXXXXX";
    int fd = ::mkstemp(tpl);
    ::close(fd);
    ::unlink(tpl);  // remove the file; we only need the unique path
    return tpl;
}

} // namespace

class UnixSocketTest : public ::testing::Test {
protected:
    std::string socket_path;

    void SetUp() override {
        socket_path = make_socket_path();
    }

    void TearDown() override {
        ::unlink(socket_path.c_str());
    }
};

// ─── Connect / accept handshake ───────────────────────────────────────────────

TEST_F(UnixSocketTest, ConnectAcceptHandshake) {
    using namespace ibridger::transport;

    UnixSocketTransport server;
    ASSERT_FALSE(server.listen(socket_path));

    std::thread server_thread([&]() {
        auto [conn, err] = server.accept();
        EXPECT_FALSE(err) << err.message();
        ASSERT_NE(conn, nullptr);
        EXPECT_TRUE(conn->is_connected());
    });

    UnixSocketTransport client_transport;
    auto [conn, err] = client_transport.connect(socket_path);
    EXPECT_FALSE(err) << err.message();
    ASSERT_NE(conn, nullptr);
    EXPECT_TRUE(conn->is_connected());

    server_thread.join();
}

// ─── Small payload roundtrip ──────────────────────────────────────────────────

TEST_F(UnixSocketTest, SendRecvSmallPayload) {
    using namespace ibridger::transport;

    UnixSocketTransport server;
    ASSERT_FALSE(server.listen(socket_path));

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

    UnixSocketTransport client_transport;
    auto [conn, err] = client_transport.connect(socket_path);
    ASSERT_FALSE(err);

    auto [sent, send_err] = conn->send(
        reinterpret_cast<const uint8_t*>(message.data()), message.size());
    EXPECT_FALSE(send_err) << send_err.message();
    EXPECT_EQ(sent, message.size());

    server_thread.join();
    EXPECT_EQ(received, message);
}

// ─── 1 MB payload (exercises partial write / read loops) ──────────────────────

TEST_F(UnixSocketTest, SendRecvLargePayload) {
    using namespace ibridger::transport;

    UnixSocketTransport server;
    ASSERT_FALSE(server.listen(socket_path));

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

    UnixSocketTransport client_transport;
    auto [conn, err] = client_transport.connect(socket_path);
    ASSERT_FALSE(err);

    auto [sent, send_err] = conn->send(payload.data(), kSize);
    EXPECT_FALSE(send_err) << send_err.message();
    EXPECT_EQ(sent, kSize);
    conn->close();

    server_thread.join();
    EXPECT_EQ(payload, received);
}

// ─── Close detection (recv returns 0 on clean EOF) ────────────────────────────

TEST_F(UnixSocketTest, CloseDetection) {
    using namespace ibridger::transport;

    UnixSocketTransport server;
    ASSERT_FALSE(server.listen(socket_path));

    std::thread server_thread([&]() {
        auto [conn, err] = server.accept();
        ASSERT_FALSE(err);
        // Server immediately closes its end.
        conn->close();
        EXPECT_FALSE(conn->is_connected());
    });

    UnixSocketTransport client_transport;
    auto [conn, err] = client_transport.connect(socket_path);
    ASSERT_FALSE(err);

    server_thread.join();

    // After the server closes, recv must return 0 with no error (clean EOF).
    uint8_t buf[16];
    auto [n, recv_err] = conn->recv(buf, sizeof(buf));
    EXPECT_EQ(n, 0u);
    EXPECT_FALSE(recv_err) << recv_err.message();
}

// ─── Multiple sequential connections ──────────────────────────────────────────

TEST_F(UnixSocketTest, MultipleSequentialConnections) {
    using namespace ibridger::transport;

    UnixSocketTransport server;
    ASSERT_FALSE(server.listen(socket_path));

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
        UnixSocketTransport client_transport;
        auto [conn, err] = client_transport.connect(socket_path);
        EXPECT_FALSE(err) << err.message();
        ASSERT_NE(conn, nullptr);
        client_ids.push_back(conn->id());
    }

    server_thread.join();

    // All connections should have been established.
    EXPECT_EQ(static_cast<int>(server_ids.size()), kConnections);
    EXPECT_EQ(static_cast<int>(client_ids.size()), kConnections);
}

#else

TEST(UnixSocketTest, SkippedOnThisPlatform) {
    GTEST_SKIP() << "Unix socket transport is not available on this platform";
}

#endif // defined(__unix__) || defined(__APPLE__)

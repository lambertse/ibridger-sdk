#include <gtest/gtest.h>
#include "ibridger/transport/named_pipe_transport.h"

namespace {

const std::string kTestEndpoint =
#ifdef _WIN32
    "\\\\.\\pipe\\ibridger_test";
#else
    "/tmp/ibridger_named_pipe_test";
#endif

} // namespace

// ─── Non-Windows: verify all operations return not_supported ──────────────────

#ifndef _WIN32

TEST(NamedPipeTest, ListenReturnsNotSupported) {
    ibridger::transport::NamedPipeTransport transport;
    auto err = transport.listen(kTestEndpoint);
    EXPECT_EQ(err, std::make_error_code(std::errc::not_supported));
}

TEST(NamedPipeTest, ConnectReturnsNotSupported) {
    ibridger::transport::NamedPipeTransport transport;
    auto [conn, err] = transport.connect(kTestEndpoint);
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

#else // _WIN32

// TODO: add the same test suite as unix_socket_test.cpp once the Windows
// implementation is complete (NamedPipeTransport::listen/accept/connect/close).

TEST(NamedPipeTest, PlaceholderWindowsTestsNotYetImplemented) {
    GTEST_SKIP() << "Windows named pipe tests not yet implemented";
}

#endif // _WIN32

#include <gtest/gtest.h>
#include "ibridger/rpc/builtin/ping_service.h"
#include "ibridger/rpc/client.h"
#include "ibridger/rpc/server.h"
#include "ibridger/rpc.pb.h"

#include <chrono>
#include <unistd.h>

using ibridger::rpc::Client;
using ibridger::rpc::ClientConfig;
using ibridger::rpc::Server;
using ibridger::rpc::ServerConfig;
using ibridger::rpc::builtin::PingService;

namespace {

std::string make_socket_path() {
    char tpl[] = "/tmp/ibridger_ping_test_XXXXXX";
    int fd = ::mkstemp(tpl);
    ::close(fd);
    ::unlink(tpl);
    return tpl;
}

struct TestServer {
    std::string path;
    Server server;

    explicit TestServer(const std::string& p, bool builtins = true)
        : path(p), server([&] {
            ServerConfig cfg;
            cfg.endpoint = p;
            cfg.register_builtins = builtins;
            return cfg;
        }()) {
        auto err = server.start();
        if (err) throw std::runtime_error("server start failed: " + err.message());
    }
    ~TestServer() {
        server.stop();
        ::unlink(path.c_str());
    }
};

} // namespace

// ─── Unit: direct handler invocation ─────────────────────────────────────────

TEST(PingService, ServiceMetadata) {
    PingService svc;
    EXPECT_EQ(svc.name(), "ibridger.Ping");
    ASSERT_EQ(svc.methods().size(), 1u);
    EXPECT_EQ(svc.methods()[0], "Ping");
}

TEST(PingService, UnknownMethodReturnsNullHandler) {
    PingService svc;
    EXPECT_EQ(svc.get_method("Unknown"), nullptr);
}

TEST(PingService, HandlerReturnsPongWithTimestamp) {
    PingService svc;
    auto handler = svc.get_method("Ping");
    ASSERT_NE(handler, nullptr);

    ibridger::Ping ping;
    ping.set_client_id("test-client");
    std::string payload;
    ASSERT_TRUE(ping.SerializeToString(&payload));

    const auto before_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto [response_payload, err] = handler(payload);

    const auto after_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    ASSERT_FALSE(err) << err.message();

    ibridger::Pong pong;
    ASSERT_TRUE(pong.ParseFromString(response_payload));

    EXPECT_EQ(pong.server_id(), "ibridger-server");
    // Timestamp should be within the window of the call.
    EXPECT_GE(pong.timestamp_ms(), before_ms);
    EXPECT_LE(pong.timestamp_ms(), after_ms + 100);
}

TEST(PingService, HandlerRejectsMalformedPayload) {
    PingService svc;
    auto handler = svc.get_method("Ping");
    ASSERT_NE(handler, nullptr);

    auto [out, err] = handler("not valid protobuf \x01\x02\x03");
    EXPECT_TRUE(err);
}

// ─── Integration: full client→server roundtrip ────────────────────────────────

TEST(PingService, IntegrationRoundtrip) {
    auto path = make_socket_path();
    TestServer srv(path);  // register_builtins=true by default

    ClientConfig cfg;
    cfg.endpoint = path;
    Client client(cfg);
    ASSERT_FALSE(client.connect());

    ibridger::Ping ping;
    ping.set_client_id("integration-test");
    std::string payload;
    ASSERT_TRUE(ping.SerializeToString(&payload));

    const auto before_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto [resp, err] = client.call("ibridger.Ping", "Ping", payload);

    ASSERT_FALSE(err) << err.message();
    EXPECT_EQ(resp.type(),   ibridger::RESPONSE);
    EXPECT_EQ(resp.status(), ibridger::OK);

    ibridger::Pong pong;
    ASSERT_TRUE(pong.ParseFromString(resp.payload()));

    EXPECT_EQ(pong.server_id(), "ibridger-server");
    EXPECT_GE(pong.timestamp_ms(), before_ms);
    // Timestamp is within 1 second of "now".
    EXPECT_LE(pong.timestamp_ms(),
              before_ms + 1000);
}

// ─── Verify builtins can be disabled ─────────────────────────────────────────

TEST(PingService, DisabledWhenRegisterBuiltinsFalse) {
    auto path = make_socket_path();
    TestServer srv(path, /*builtins=*/false);

    ClientConfig cfg;
    cfg.endpoint = path;
    Client client(cfg);
    ASSERT_FALSE(client.connect());

    ibridger::Ping ping;
    std::string payload;
    ASSERT_TRUE(ping.SerializeToString(&payload));

    auto [resp, err] = client.call("ibridger.Ping", "Ping", payload);
    ASSERT_FALSE(err);
    EXPECT_EQ(resp.type(),   ibridger::ERROR);
    EXPECT_EQ(resp.status(), ibridger::NOT_FOUND);
}

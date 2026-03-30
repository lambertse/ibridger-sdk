#include <gtest/gtest.h>
#include "ibridger/rpc/server.h"
#include "ibridger/rpc/service.h"
#include "ibridger/protocol/envelope_codec.h"
#include "ibridger/protocol/framing.h"
#include "ibridger/transport/unix_socket_transport.h"

#include <thread>
#include <unistd.h>
#include <vector>

using ibridger::rpc::IService;
using ibridger::rpc::MethodHandler;
using ibridger::rpc::Server;
using ibridger::rpc::ServerConfig;
using ibridger::protocol::EnvelopeCodec;
using ibridger::protocol::FramedConnection;
using ibridger::transport::UnixSocketTransport;

namespace {

// ─── Helpers ──────────────────────────────────────────────────────────────────

std::string make_socket_path() {
    char tpl[] = "/tmp/ibridger_server_test_XXXXXX";
    int fd = ::mkstemp(tpl);
    ::close(fd);
    ::unlink(tpl);
    return tpl;
}

/// Minimal echo service: Echo → returns payload unchanged.
class EchoService : public IService {
public:
    std::string name() const override { return "EchoService"; }
    std::vector<std::string> methods() const override { return {"Echo"}; }
    MethodHandler get_method(const std::string& m) const override {
        if (m == "Echo") {
            return [](const std::string& payload)
                -> std::pair<std::string, std::error_code> {
                return {payload, {}};
            };
        }
        return nullptr;
    }
};

/// Connect a raw codec client to the server at `endpoint`.
/// Caller owns the returned EnvelopeCodec (holds the connection alive).
std::pair<EnvelopeCodec, std::unique_ptr<UnixSocketTransport>>
connect_client(const std::string& endpoint) {
    auto transport = std::make_unique<UnixSocketTransport>();
    auto [conn, err] = transport->connect(endpoint);
    if (err) throw std::runtime_error("connect failed: " + err.message());
    auto framed = std::make_shared<FramedConnection>(std::move(conn));
    return {EnvelopeCodec(framed), std::move(transport)};
}

/// Send a REQUEST and return the RESPONSE envelope.
ibridger::Envelope call(EnvelopeCodec& codec,
                        const std::string& service,
                        const std::string& method,
                        const std::string& payload,
                        uint64_t id = 1) {
    ibridger::Envelope req;
    req.set_type(ibridger::REQUEST);
    req.set_request_id(id);
    req.set_service_name(service);
    req.set_method_name(method);
    req.set_payload(payload);
    if (auto err = codec.send(req); err)
        throw std::runtime_error("send failed: " + err.message());
    auto [resp, err] = codec.recv();
    if (err) throw std::runtime_error("recv failed: " + err.message());
    return resp;
}

} // namespace

// ─── Start / stop lifecycle ───────────────────────────────────────────────────

TEST(Server, StartStopLifecycle) {
    ServerConfig cfg;
    cfg.endpoint = make_socket_path();

    Server server(cfg);
    server.register_service(std::make_shared<EchoService>());

    ASSERT_FALSE(server.start());
    EXPECT_TRUE(server.is_running());

    server.stop();
    EXPECT_FALSE(server.is_running());

    ::unlink(cfg.endpoint.c_str());
}

// ─── Single request roundtrip ─────────────────────────────────────────────────

TEST(Server, SingleRequestRoundtrip) {
    ServerConfig cfg;
    cfg.endpoint = make_socket_path();

    Server server(cfg);
    server.register_service(std::make_shared<EchoService>());
    ASSERT_FALSE(server.start());

    auto [codec, transport] = connect_client(cfg.endpoint);
    auto resp = call(codec, "EchoService", "Echo", "hello server", 42);

    EXPECT_EQ(resp.type(),       ibridger::RESPONSE);
    EXPECT_EQ(resp.status(),     ibridger::OK);
    EXPECT_EQ(resp.payload(),    "hello server");
    EXPECT_EQ(resp.request_id(), 42ULL);

    server.stop();
    ::unlink(cfg.endpoint.c_str());
}

// ─── Unknown service returns NOT_FOUND ───────────────────────────────────────

TEST(Server, UnknownServiceReturnsNotFound) {
    ServerConfig cfg;
    cfg.endpoint = make_socket_path();

    Server server(cfg);
    server.register_service(std::make_shared<EchoService>());
    ASSERT_FALSE(server.start());

    auto [codec, transport] = connect_client(cfg.endpoint);
    auto resp = call(codec, "ghost.Service", "DoThing", "", 7);

    EXPECT_EQ(resp.type(),       ibridger::ERROR);
    EXPECT_EQ(resp.status(),     ibridger::NOT_FOUND);
    EXPECT_EQ(resp.request_id(), 7ULL);

    server.stop();
    ::unlink(cfg.endpoint.c_str());
}

// ─── Multiple concurrent connections ─────────────────────────────────────────

TEST(Server, MultipleConcurrentConnections) {
    ServerConfig cfg;
    cfg.endpoint = make_socket_path();

    Server server(cfg);
    server.register_service(std::make_shared<EchoService>());
    ASSERT_FALSE(server.start());

    constexpr int kClients   = 3;
    constexpr int kRequests  = 10;
    std::atomic<int> success_count{0};

    auto client_fn = [&]() {
        try {
            auto [codec, transport] = connect_client(cfg.endpoint);
            for (int i = 0; i < kRequests; i++) {
                std::string payload = "msg-" + std::to_string(i);
                auto resp = call(codec, "EchoService", "Echo", payload,
                                 static_cast<uint64_t>(i));
                if (resp.type()    == ibridger::RESPONSE &&
                    resp.status()  == ibridger::OK &&
                    resp.payload() == payload) {
                    success_count++;
                }
            }
        } catch (...) {}
    };

    std::vector<std::thread> clients;
    for (int i = 0; i < kClients; i++) clients.emplace_back(client_fn);
    for (auto& t : clients) t.join();

    EXPECT_EQ(success_count.load(), kClients * kRequests);

    server.stop();
    ::unlink(cfg.endpoint.c_str());
}

// ─── Client disconnect doesn't crash server ───────────────────────────────────

TEST(Server, ClientDisconnectDoesNotCrashServer) {
    ServerConfig cfg;
    cfg.endpoint = make_socket_path();

    Server server(cfg);
    server.register_service(std::make_shared<EchoService>());
    ASSERT_FALSE(server.start());

    // Connect and immediately disconnect 3 clients.
    for (int i = 0; i < 3; i++) {
        auto [codec, transport] = connect_client(cfg.endpoint);
        // codec goes out of scope here, closing the connection.
    }

    // Give handler threads a moment to process the disconnects.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Server should still be running and accepting new connections.
    EXPECT_TRUE(server.is_running());

    // A new client should still work.
    auto [codec, transport] = connect_client(cfg.endpoint);
    auto resp = call(codec, "EchoService", "Echo", "still alive");
    EXPECT_EQ(resp.type(),    ibridger::RESPONSE);
    EXPECT_EQ(resp.payload(), "still alive");

    server.stop();
    ::unlink(cfg.endpoint.c_str());
}

// ─── Server stop while clients are connected ──────────────────────────────────

TEST(Server, StopWhileClientsConnected) {
    ServerConfig cfg;
    cfg.endpoint = make_socket_path();

    Server server(cfg);
    server.register_service(std::make_shared<EchoService>());
    ASSERT_FALSE(server.start());

    // Connect 3 clients and keep them open (no requests sent).
    auto [c1, t1] = connect_client(cfg.endpoint);
    auto [c2, t2] = connect_client(cfg.endpoint);
    auto [c3, t3] = connect_client(cfg.endpoint);

    // Give server time to accept all connections.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // stop() must return cleanly even with live connections.
    server.stop();
    EXPECT_FALSE(server.is_running());

    ::unlink(cfg.endpoint.c_str());
}

#include "ibridger/rpc/client.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "ibridger/common/error.h"
#include "ibridger/rpc/server.h"
#include "ibridger/rpc/service.h"
#include "test_transport_pair.h"

using ibridger::rpc::Client;
using ibridger::rpc::ClientConfig;
using ibridger::rpc::IService;
using ibridger::rpc::MethodHandler;
using ibridger::rpc::Server;
using ibridger::rpc::ServerConfig;

namespace {

class EchoService : public IService {
 public:
  std::string name() const override { return "EchoService"; }
  std::vector<std::string> methods() const override { return {"Echo"}; }
  MethodHandler get_method(const std::string& m) const override {
    if (m == "Echo") {
      return
          [](const std::string& p) -> std::pair<std::string, std::error_code> {
            return {p, {}};
          };
    }
    return nullptr;
  }
};

/// RAII helper: starts a server on construction, stops on destruction.
struct TestServer {
  std::string path;
  Server server;

  explicit TestServer(const std::string& p)
      : path(p), server([&] {
          ServerConfig cfg;
          cfg.endpoint = p;
          return cfg;
        }()) {
    server.register_service(std::make_shared<EchoService>());
    auto err = server.start();
    if (err) throw std::runtime_error("server start failed: " + err.message());
  }
  ~TestServer() {
    server.stop();
    ibridger::test::cleanup_endpoint(path);
  }
};

}  // namespace

// ─── Connect / disconnect lifecycle ──────────────────────────────────────────

TEST(Client, ConnectDisconnectLifecycle) {
  auto path = ibridger::test::make_endpoint();
  TestServer srv(path);

  ClientConfig cfg;
  cfg.endpoint = path;
  Client client(cfg);

  EXPECT_FALSE(client.is_connected());

  ASSERT_FALSE(client.connect());
  EXPECT_TRUE(client.is_connected());

  client.disconnect();
  EXPECT_FALSE(client.is_connected());
}

// ─── Successful call roundtrip
// ────────────────────────────────────────────────

TEST(Client, SuccessfulCallRoundtrip) {
  auto path = ibridger::test::make_endpoint();
  TestServer srv(path);

  ClientConfig cfg;
  cfg.endpoint = path;
  Client client(cfg);
  ASSERT_FALSE(client.connect());

  auto [resp, err] = client.call("EchoService", "Echo", "hello client");

  ASSERT_FALSE(err) << err.message();
  EXPECT_EQ(resp.type(), ibridger::RESPONSE);
  EXPECT_EQ(resp.status(), ibridger::OK);
  EXPECT_EQ(resp.payload(), "hello client");
}

// ─── NOT_FOUND propagation
// ────────────────────────────────────────────────────

TEST(Client, NotFoundErrorPropagation) {
  auto path = ibridger::test::make_endpoint();
  TestServer srv(path);

  ClientConfig cfg;
  cfg.endpoint = path;
  Client client(cfg);
  ASSERT_FALSE(client.connect());

  auto [resp, err] = client.call("ghost.Service", "DoThing", "");

  // Transport succeeded — the server replied with an error Envelope.
  ASSERT_FALSE(err) << err.message();
  EXPECT_EQ(resp.type(), ibridger::ERROR);
  EXPECT_EQ(resp.status(), ibridger::NOT_FOUND);
  EXPECT_FALSE(resp.error_message().empty());
}

// ─── Connection refused when no server running
// ────────────────────────────────

TEST(Client, ConnectionRefusedWhenNoServer) {
  ClientConfig cfg;
  cfg.endpoint = ibridger::test::make_endpoint();  // unique path, no server
  Client client(cfg);

  auto err = client.connect();
  EXPECT_TRUE(err) << "Expected connection error, got success";
  EXPECT_FALSE(client.is_connected());
}

// ─── Multiple sequential calls on same connection
// ─────────────────────────────

TEST(Client, MultipleSequentialCalls) {
  auto path = ibridger::test::make_endpoint();
  TestServer srv(path);

  ClientConfig cfg;
  cfg.endpoint = path;
  Client client(cfg);
  ASSERT_FALSE(client.connect());

  for (int i = 0; i < 10; i++) {
    std::string payload = "msg-" + std::to_string(i);
    auto [resp, err] = client.call("EchoService", "Echo", payload);

    ASSERT_FALSE(err) << "call " << i << " failed: " << err.message();
    EXPECT_EQ(resp.type(), ibridger::RESPONSE);
    EXPECT_EQ(resp.status(), ibridger::OK);
    EXPECT_EQ(resp.payload(), payload);
    // request_id increments with each call
    EXPECT_EQ(resp.request_id(), static_cast<uint64_t>(i + 1));
  }
}

// ─── call() before connect returns not_connected ─────────────────────────────

TEST(Client, CallBeforeConnectReturnsError) {
  ClientConfig cfg;
  cfg.endpoint =
      "/tmp/irrelevant";  // never used — call() checks connected first
  Client client(cfg);

  auto [resp, err] = client.call("EchoService", "Echo", "hi");
  EXPECT_EQ(err, ibridger::common::make_error_code(
                     ibridger::common::Error::not_connected));
}

// ─── Reconnect after disconnect
// ───────────────────────────────────────────────

TEST(Client, ReconnectAfterDisconnect) {
  auto path = ibridger::test::make_endpoint();
  TestServer srv(path);

  ClientConfig cfg;
  cfg.endpoint = path;
  Client client(cfg);

  ASSERT_FALSE(client.connect());
  auto [r1, e1] = client.call("EchoService", "Echo", "first");
  ASSERT_FALSE(e1);
  EXPECT_EQ(r1.payload(), "first");

  client.disconnect();
  EXPECT_FALSE(client.is_connected());

  ASSERT_FALSE(client.connect());
  auto [r2, e2] = client.call("EchoService", "Echo", "second");
  ASSERT_FALSE(e2);
  EXPECT_EQ(r2.payload(), "second");
}

// ─── on_disconnect fires when server dies ────────────────────────────────────

TEST(Client, OnDisconnectFiresWhenServerDies) {
  auto path = ibridger::test::make_endpoint();

  ClientConfig cfg;
  cfg.endpoint = path;

  std::atomic<int> fired{0};
  cfg.on_disconnect = [&] { fired++; };

  Client client(cfg);

  {
    TestServer srv(path);
    ASSERT_FALSE(client.connect());
    EXPECT_TRUE(client.is_connected());
    // srv destructor stops the server here
  }

  // Give the OS a moment to deliver the close event via the next call.
  auto [resp, err] = client.call("EchoService", "Echo", "ping");
  EXPECT_TRUE(err);                     // transport error
  EXPECT_FALSE(client.is_connected());  // codec was nulled
  EXPECT_EQ(fired.load(), 1);           // callback fired exactly once
}

// ─── Reconnect: call() blocks and retries after server restart ───────────────

TEST(Client, ReconnectSucceedsAfterServerRestart) {
  auto path = ibridger::test::make_endpoint();

  ibridger::rpc::ReconnectConfig rc;
  rc.max_attempts = 20;
  rc.base_delay = std::chrono::milliseconds(50);
  rc.max_delay = std::chrono::milliseconds(200);

  std::atomic<int> reconnected{0};
  rc.on_reconnect = [&] { reconnected++; };

  ClientConfig cfg;
  cfg.endpoint = path;
  cfg.reconnect = rc;

  Client client(cfg);

  // First connection.
  {
    TestServer srv(path);
    ASSERT_FALSE(client.connect());
    auto [r1, e1] = client.call("EchoService", "Echo", "before");
    ASSERT_FALSE(e1);
    EXPECT_EQ(r1.payload(), "before");
    // srv destructor stops the server here
  }

  // Restart the server in a background thread after a short delay.
  std::thread restarter([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    // TestServer destructor cleaned up the socket file; start a new one.
    TestServer srv2(path);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // srv2 stays alive for the duration of the reconnect call
  });

  // call() should block, reconnect, then succeed.
  auto [r2, e2] = client.call("EchoService", "Echo", "after");
  ASSERT_FALSE(e2) << e2.message();
  EXPECT_EQ(r2.payload(), "after");
  EXPECT_EQ(reconnected.load(), 1);

  restarter.join();
}

// ─── Reconnect: call() fails after max_attempts exhausted ────────────────────

TEST(Client, ReconnectFailsAfterMaxAttempts) {
  auto path = ibridger::test::make_endpoint();

  ibridger::rpc::ReconnectConfig rc;
  rc.max_attempts = 3;
  rc.base_delay = std::chrono::milliseconds(10);
  rc.max_delay = std::chrono::milliseconds(10);

  ClientConfig cfg;
  cfg.endpoint = path;
  cfg.reconnect = rc;

  Client client(cfg);
  // Never connect — server doesn't exist.

  auto [resp, err] = client.call("EchoService", "Echo", "lost");
  EXPECT_EQ(err, ibridger::common::make_error_code(
                     ibridger::common::Error::not_connected));
}

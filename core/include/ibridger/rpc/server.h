#pragma once

#include "ibridger/rpc/service.h"
#include "ibridger/rpc/service_registry.h"
#include "ibridger/rpc/dispatcher.h"
#include "ibridger/transport/transport_factory.h"
#include "ibridger/protocol/framing.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace ibridger {
namespace rpc {

struct ServerConfig {
    std::string endpoint;
    transport::TransportType transport = transport::TransportType::kAuto;
    size_t max_connections = 64;
    std::chrono::seconds idle_timeout{300};
    bool register_builtins = true;  // reserved for Phase 14
};

/// RPC server that accepts connections and dispatches Envelope requests.
///
/// Threading model:
///   - One dedicated accept-loop thread (spawned by start()).
///   - One handler thread per accepted connection.
///   - stop() blocks until all threads have exited.
class Server {
public:
    explicit Server(ServerConfig config);
    ~Server();

    /// Register a service before or after start() (thread-safe).
    void register_service(std::shared_ptr<IService> service);

    /// Bind, listen, and spawn the accept-loop thread. Non-blocking.
    std::error_code start();

    /// Graceful shutdown: stop accepting, close all connections, join threads.
    void stop();

    bool is_running() const;

private:
    void accept_loop();
    void handle_connection(std::shared_ptr<protocol::FramedConnection> framed);
    void remove_connection(protocol::FramedConnection* ptr);

    ServerConfig config_;
    std::shared_ptr<ServiceRegistry> registry_;
    std::unique_ptr<Dispatcher> dispatcher_;
    std::unique_ptr<transport::ITransport> transport_;

    std::atomic<bool> running_{false};
    std::thread accept_thread_;

    mutable std::mutex connections_mutex_;
    std::vector<std::shared_ptr<protocol::FramedConnection>> active_connections_;
    std::vector<std::thread> handler_threads_;
};

} // namespace rpc
} // namespace ibridger

#include "ibridger/rpc/server.h"
#include "ibridger/protocol/envelope_codec.h"

#include <algorithm>

namespace ibridger {
namespace rpc {

Server::Server(ServerConfig config)
    : config_(std::move(config))
    , registry_(std::make_shared<ServiceRegistry>())
    , dispatcher_(std::make_unique<Dispatcher>(registry_)) {}

Server::~Server() {
    stop();
}

void Server::register_service(std::shared_ptr<IService> service) {
    registry_->register_service(std::move(service));
}

// ─── start ────────────────────────────────────────────────────────────────────

std::error_code Server::start() {
    if (running_) return std::make_error_code(std::errc::already_connected);

    transport_ = transport::TransportFactory::create(config_.transport);
    if (!transport_) {
        return std::make_error_code(std::errc::not_supported);
    }

    if (auto err = transport_->listen(config_.endpoint)) return err;

    running_ = true;
    accept_thread_ = std::thread(&Server::accept_loop, this);
    return {};
}

// ─── stop ─────────────────────────────────────────────────────────────────────

void Server::stop() {
    if (!running_.exchange(false)) return;

    // Unblock the accept() call.
    if (transport_) transport_->close();
    if (accept_thread_.joinable()) accept_thread_.join();

    // Close all active connections to unblock handler recv() calls.
    std::vector<std::shared_ptr<protocol::FramedConnection>> snapshot;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        snapshot = active_connections_;
    }
    for (auto& framed : snapshot) framed->close();

    // Join all handler threads.
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        threads = std::move(handler_threads_);
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

bool Server::is_running() const {
    return running_;
}

// ─── accept_loop ──────────────────────────────────────────────────────────────

void Server::accept_loop() {
    while (running_) {
        auto [conn, err] = transport_->accept();
        if (err || !conn) break;   // error or server was closed

        // Reject if at connection limit.
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            if (active_connections_.size() >= config_.max_connections) {
                conn->close();
                continue;
            }
        }

        auto framed = std::make_shared<protocol::FramedConnection>(std::move(conn));

        std::lock_guard<std::mutex> lock(connections_mutex_);
        active_connections_.push_back(framed);
        handler_threads_.emplace_back(
            [this, framed]() { handle_connection(framed); });
    }
}

// ─── handle_connection ────────────────────────────────────────────────────────

void Server::handle_connection(std::shared_ptr<protocol::FramedConnection> framed) {
    protocol::EnvelopeCodec codec(framed);

    for (;;) {
        auto [request, err] = codec.recv();
        if (err) break;

        auto response = dispatcher_->dispatch(request);
        if (auto send_err = codec.send(response); send_err) break;
    }

    remove_connection(framed.get());
}

// ─── remove_connection ────────────────────────────────────────────────────────

void Server::remove_connection(protocol::FramedConnection* ptr) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = std::find_if(
        active_connections_.begin(), active_connections_.end(),
        [ptr](const auto& p) { return p.get() == ptr; });
    if (it != active_connections_.end()) {
        active_connections_.erase(it);
    }
}

} // namespace rpc
} // namespace ibridger

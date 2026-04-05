#include "ibridger/rpc/client.h"

#include <thread>

#include "ibridger/common/error.h"
#include "ibridger/common/logger.h"

namespace ibridger {
namespace rpc {

Client::Client(ClientConfig config) : config_(std::move(config)) {}

Client::~Client() { disconnect(); }

// ─── connect
// ──────────────────────────────────────────────────────────────────

std::error_code Client::connect() {
  std::unique_lock<std::mutex> lock(mutex_);
  return connect_locked();
}

std::error_code Client::connect_locked() {
  if (codec_) return common::make_error_code(common::Error::already_connected);

  transport_ = transport::TransportFactory::create(config_.transport);
  if (!transport_) return common::make_error_code(common::Error::internal);

  auto [conn, err] = transport_->connect(config_.endpoint);
  if (err) {
    common::Logger::error("Client: connect failed: " + err.message());
    transport_.reset();
    return err;
  }

  framed_ = std::make_shared<protocol::FramedConnection>(std::move(conn));
  codec_ = std::make_unique<protocol::EnvelopeCodec>(framed_);
  common::Logger::info("Client: connected to " + config_.endpoint);
  return {};
}

// ─── disconnect
// ───────────────────────────────────────────────────────────────

void Client::disconnect() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (framed_) {
    framed_->close();
    framed_.reset();
    common::Logger::info("Client: disconnected");
  }
  codec_.reset();
  transport_.reset();
}

// ─── is_connected
// ─────────────────────────────────────────────────────────────

bool Client::is_connected() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return framed_ && framed_->is_connected();
}

// ─── handle_disconnect
// ────────────────────────────────────────────────────────

void Client::handle_disconnect(std::unique_lock<std::mutex>& lock) {
  if (framed_) framed_->close();
  framed_.reset();
  codec_.reset();
  transport_.reset();

  // Fire the callback outside the lock to avoid deadlocks if the
  // callback calls back into the client.
  auto cb = config_.on_disconnect;
  lock.unlock();
  if (cb) cb();
  lock.lock();
}

// ─── attempt_reconnect
// ────────────────────────────────────────────────────────

std::error_code Client::attempt_reconnect(std::unique_lock<std::mutex>& lock) {
  const auto& rc = config_.reconnect.value();
  const int max =
      (rc.max_attempts < 0) ? std::numeric_limits<int>::max() : rc.max_attempts;

  for (int attempt = 0; attempt < max; attempt++) {
    // Exponential backoff — compute delay while lock is held, then release.
    const long long ms = rc.base_delay.count() * (1LL << std::min(attempt, 30));
    const auto delay = std::chrono::milliseconds(
        std::min(ms, static_cast<long long>(rc.max_delay.count())));

    lock.unlock();
    std::this_thread::sleep_for(delay);
    lock.lock();

    if (auto err = connect_locked(); !err) {
      if (rc.on_reconnect) rc.on_reconnect();
      return {};
    }
  }
  return common::make_error_code(common::Error::not_connected);
}

// ─── call
// ─────────────────────────────────────────────────────────────────────

std::pair<ibridger::Envelope, std::error_code> Client::call(
    const std::string& service, const std::string& method,
    const std::string& payload) {
  std::unique_lock<std::mutex> lock(mutex_);

  // If disconnected and reconnect is configured, attempt to restore the
  // connection before proceeding (mirrors the JS waitForReconnect behaviour).
  if (!codec_ && config_.reconnect) {
    if (auto err = attempt_reconnect(lock); err) {
      return {ibridger::Envelope{},
              common::make_error_code(common::Error::not_connected)};
    }
  }

  if (!codec_) {
    return {ibridger::Envelope{},
            common::make_error_code(common::Error::not_connected)};
  }

  const uint64_t id = next_request_id_++;

  ibridger::Envelope request;
  request.set_type(ibridger::REQUEST);
  request.set_request_id(id);
  request.set_service_name(service);
  request.set_method_name(method);
  request.set_payload(payload);

  if (auto err = codec_->send(request); err) {
    // send failed → server never received it → safe to retry once.
    handle_disconnect(lock);
    if (config_.reconnect) {
      if (!attempt_reconnect(lock)) {
        if (auto err2 = codec_->send(request); err2) {
          handle_disconnect(lock);
          return {ibridger::Envelope{}, err2};
        }
        // fall through to recv
      } else {
        return {ibridger::Envelope{}, err};
      }
    } else {
      return {ibridger::Envelope{}, err};
    }
  }

  auto [response, recv_err] = codec_->recv();
  if (recv_err) {
    // recv failed → server may have processed the request → don't retry.
    handle_disconnect(lock);
    return {ibridger::Envelope{}, recv_err};
  }

  // Validate correlation — a mismatch indicates a protocol bug.
  if (response.request_id() != id) {
    common::Logger::error("Client: request_id mismatch (sent " +
                          std::to_string(id) + ", got " +
                          std::to_string(response.request_id()) + ")");
    return {ibridger::Envelope{},
            common::make_error_code(common::Error::protocol_error)};
  }

  return {std::move(response), {}};
}

}  // namespace rpc
}  // namespace ibridger

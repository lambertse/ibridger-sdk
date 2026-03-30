#include "ibridger/protocol/framing.h"

#include <cerrno>
#include <cstdint>

namespace ibridger {
namespace protocol {

FramedConnection::FramedConnection(std::unique_ptr<transport::IConnection> conn)
    : conn_(std::move(conn)) {}

// ─── send_frame ───────────────────────────────────────────────────────────────

std::error_code FramedConnection::send_frame(const std::string& data) {
    if (data.size() > kMaxFrameSize) {
        return std::make_error_code(std::errc::message_size);
    }

    // Encode payload length as 4-byte big-endian header.
    auto len = static_cast<uint32_t>(data.size());
    uint8_t header[4] = {
        static_cast<uint8_t>((len >> 24) & 0xFF),
        static_cast<uint8_t>((len >> 16) & 0xFF),
        static_cast<uint8_t>((len >>  8) & 0xFF),
        static_cast<uint8_t>((len >>  0) & 0xFF),
    };

    if (auto [sent, err] = conn_->send(header, 4); err) {
        return err;
    }

    if (!data.empty()) {
        if (auto [sent, err] = conn_->send(
                reinterpret_cast<const uint8_t*>(data.data()), data.size()); err) {
            return err;
        }
    }

    return {};
}

// ─── recv_frame ───────────────────────────────────────────────────────────────

std::pair<std::string, std::error_code> FramedConnection::recv_frame() {
    // Read the 4-byte length header.
    uint8_t header[4];
    if (auto err = read_exact(header, 4)) {
        return {"", err};
    }

    uint32_t len =
        (static_cast<uint32_t>(header[0]) << 24) |
        (static_cast<uint32_t>(header[1]) << 16) |
        (static_cast<uint32_t>(header[2]) <<  8) |
        (static_cast<uint32_t>(header[3]) <<  0);

    if (len > kMaxFrameSize) {
        return {"", std::make_error_code(std::errc::message_size)};
    }

    if (len == 0) {
        return {"", {}};
    }

    // Read exactly `len` payload bytes.
    std::string payload(len, '\0');
    if (auto err = read_exact(reinterpret_cast<uint8_t*>(payload.data()), len)) {
        return {"", err};
    }

    return {std::move(payload), {}};
}

// ─── is_connected ─────────────────────────────────────────────────────────────

void FramedConnection::close() {
    if (conn_) conn_->close();
}

bool FramedConnection::is_connected() const {
    return conn_ && conn_->is_connected();
}

// ─── read_exact ───────────────────────────────────────────────────────────────

std::error_code FramedConnection::read_exact(uint8_t* buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        auto [received, err] = conn_->recv(buf + total, n - total);
        if (err) return err;
        if (received == 0) {
            // Clean EOF before all bytes arrived — peer closed mid-frame.
            return std::make_error_code(std::errc::connection_reset);
        }
        total += received;
    }
    return {};
}

} // namespace protocol
} // namespace ibridger

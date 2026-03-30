#include "ibridger/rpc/builtin/ping_service.h"
#include "ibridger/rpc.pb.h"

#include <chrono>

namespace ibridger {
namespace rpc {
namespace builtin {

std::string PingService::name() const {
    return "ibridger.Ping";
}

std::vector<std::string> PingService::methods() const {
    return {"Ping"};
}

MethodHandler PingService::get_method(const std::string& method) const {
    if (method != "Ping") return nullptr;

    return [](const std::string& payload) -> std::pair<std::string, std::error_code> {
        ibridger::Ping ping;
        if (!ping.ParseFromString(payload)) {
            return {{}, std::make_error_code(std::errc::bad_message)};
        }

        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        ibridger::Pong pong;
        pong.set_server_id("ibridger-server");
        pong.set_timestamp_ms(static_cast<int64_t>(now_ms));

        std::string out;
        if (!pong.SerializeToString(&out)) {
            return {{}, std::make_error_code(std::errc::io_error)};
        }
        return {std::move(out), {}};
    };
}

} // namespace builtin
} // namespace rpc
} // namespace ibridger

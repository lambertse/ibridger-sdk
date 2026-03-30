#pragma once

#include "ibridger/rpc/service.h"

namespace ibridger {
namespace rpc {
namespace builtin {

/// Built-in health-check service.
///
/// Service name : "ibridger.Ping"
/// Method       : "Ping"  — accepts a serialized ibridger::Ping proto,
///                          returns a serialized ibridger::Pong proto
///                          with a monotonic timestamp_ms (epoch ms).
///
/// Auto-registered by Server when ServerConfig::register_builtins == true.
class PingService : public IService {
public:
    std::string name() const override;
    std::vector<std::string> methods() const override;
    MethodHandler get_method(const std::string& method) const override;
};

} // namespace builtin
} // namespace rpc
} // namespace ibridger

#pragma once

#include "ibridger/transport/transport.h"

#include <memory>

namespace ibridger {
namespace transport {

enum class TransportType {
    kAuto,       ///< Platform default: Unix socket on macOS/Linux, Named pipe on Windows.
    kUnixSocket, ///< AF_UNIX stream socket (macOS / Linux only).
    kNamedPipe,  ///< Windows Named Pipe (Windows only).
};

class TransportFactory {
public:
    /// Create a transport of the requested type.
    ///
    /// Returns nullptr if the requested type is not supported on the current
    /// platform (e.g. kNamedPipe on Linux, kUnixSocket on Windows).
    /// kAuto always succeeds.
    static std::unique_ptr<ITransport> create(TransportType type = TransportType::kAuto);
};

} // namespace transport
} // namespace ibridger

#include "ibridger/transport/transport_factory.h"

#if defined(__unix__) || defined(__APPLE__)
#  include "ibridger/transport/unix_socket_transport.h"
#endif
#ifdef _WIN32
#  include "ibridger/transport/named_pipe_transport.h"
#endif

namespace ibridger {
namespace transport {

std::unique_ptr<ITransport> TransportFactory::create(TransportType type) {
    switch (type) {
        case TransportType::kAuto:
#if defined(__unix__) || defined(__APPLE__)
            return std::make_unique<UnixSocketTransport>();
#elif defined(_WIN32)
            return std::make_unique<NamedPipeTransport>();
#else
            return nullptr;
#endif

        case TransportType::kUnixSocket:
#if defined(__unix__) || defined(__APPLE__)
            return std::make_unique<UnixSocketTransport>();
#else
            return nullptr;
#endif

        case TransportType::kNamedPipe:
#ifdef _WIN32
            return std::make_unique<NamedPipeTransport>();
#else
            return nullptr;
#endif
    }

    return nullptr; // unreachable, silences -Wreturn-type
}

} // namespace transport
} // namespace ibridger

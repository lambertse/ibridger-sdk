#pragma once

#include <string>
#include <system_error>

namespace ibridger {
namespace common {

/// ibridger-specific error codes.
enum class Error {
    ok = 0,
    not_connected,
    already_connected,
    already_registered,       ///< service already registered in the registry
    service_not_found,
    method_not_found,
    protocol_error,
    frame_too_large,
    serialization_error,
    timeout,
    internal,
};

/// Returns the singleton std::error_category for ibridger errors.
const std::error_category& ibridger_category();

/// Convenience: make an std::error_code from an ibridger::common::Error.
inline std::error_code make_error_code(Error e) {
    return {static_cast<int>(e), ibridger_category()};
}

} // namespace common
} // namespace ibridger

/// Allow ibridger::common::Error to be used directly with std::error_code.
namespace std {
template <>
struct is_error_code_enum<ibridger::common::Error> : true_type {};
} // namespace std

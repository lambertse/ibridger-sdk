#include <gtest/gtest.h>
#include "ibridger/transport/transport_factory.h"

#if defined(__unix__) || defined(__APPLE__)
#  include "ibridger/transport/unix_socket_transport.h"
#endif
#ifdef _WIN32
#  include "ibridger/transport/named_pipe_transport.h"
#endif

using namespace ibridger::transport;

// ─── kAuto ────────────────────────────────────────────────────────────────────

TEST(TransportFactory, AutoAlwaysSucceeds) {
    auto transport = TransportFactory::create();
    EXPECT_NE(transport, nullptr);
}

TEST(TransportFactory, AutoSelectsPlatformNativeType) {
    auto transport = TransportFactory::create(TransportType::kAuto);
    ASSERT_NE(transport, nullptr);

#if defined(__unix__) || defined(__APPLE__)
    EXPECT_NE(dynamic_cast<UnixSocketTransport*>(transport.get()), nullptr)
        << "Expected UnixSocketTransport on Unix/macOS";
#elif defined(_WIN32)
    EXPECT_NE(dynamic_cast<NamedPipeTransport*>(transport.get()), nullptr)
        << "Expected NamedPipeTransport on Windows";
#endif
}

// ─── kUnixSocket ──────────────────────────────────────────────────────────────

TEST(TransportFactory, ExplicitUnixSocketType) {
    auto transport = TransportFactory::create(TransportType::kUnixSocket);

#if defined(__unix__) || defined(__APPLE__)
    ASSERT_NE(transport, nullptr);
    EXPECT_NE(dynamic_cast<UnixSocketTransport*>(transport.get()), nullptr);
#else
    EXPECT_EQ(transport, nullptr) << "kUnixSocket should return nullptr on non-Unix";
#endif
}

// ─── kNamedPipe ───────────────────────────────────────────────────────────────

TEST(TransportFactory, ExplicitNamedPipeType) {
    auto transport = TransportFactory::create(TransportType::kNamedPipe);

#ifdef _WIN32
    ASSERT_NE(transport, nullptr);
    EXPECT_NE(dynamic_cast<NamedPipeTransport*>(transport.get()), nullptr);
#else
    EXPECT_EQ(transport, nullptr) << "kNamedPipe should return nullptr on non-Windows";
#endif
}

// ─── Unsupported type on current platform ─────────────────────────────────────

TEST(TransportFactory, UnsupportedTypeReturnsNullptr) {
#if defined(__unix__) || defined(__APPLE__)
    auto transport = TransportFactory::create(TransportType::kNamedPipe);
    EXPECT_EQ(transport, nullptr);
#elif defined(_WIN32)
    auto transport = TransportFactory::create(TransportType::kUnixSocket);
    EXPECT_EQ(transport, nullptr);
#endif
}

// ─── Factory creates independent instances ────────────────────────────────────

TEST(TransportFactory, CreatesIndependentInstances) {
    auto t1 = TransportFactory::create();
    auto t2 = TransportFactory::create();
    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t2, nullptr);
    EXPECT_NE(t1.get(), t2.get());
}

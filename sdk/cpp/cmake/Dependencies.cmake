include(FetchContent)

# ─── Protobuf ─────────────────────────────────────────────────────────────────
# Prefer a system/Homebrew installation; fall back to FetchContent for CI.
find_package(protobuf CONFIG QUIET)
if(NOT protobuf_FOUND)
  message(STATUS "protobuf not found via find_package — fetching via FetchContent")
  set(protobuf_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(protobuf_INSTALL        OFF CACHE BOOL "" FORCE)
  set(protobuf_WITH_ZLIB      OFF CACHE BOOL "" FORCE)
  # Let protobuf manage its own abseil dependency ("module" avoids package detection issues)
  set(protobuf_ABSL_PROVIDER  "module" CACHE STRING "" FORCE)
  set(ABSL_PROPAGATE_CXX_STD  ON CACHE BOOL "" FORCE)

  FetchContent_Declare(
    protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG        v28.3
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(protobuf)
else()
  message(STATUS "Using system protobuf: ${protobuf_VERSION}")
endif()

# ─── GoogleTest ───────────────────────────────────────────────────────────────
if(IBRIDGER_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST           OFF CACHE BOOL "" FORCE)  # don't pollute cmake --install
  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)
endif()

# ─── Google Benchmark ─────────────────────────────────────────────────────────
if(IBRIDGER_BUILD_BENCHMARKS)
  set(BENCHMARK_ENABLE_TESTING          OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL          OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_DOWNLOAD_DEPENDENCIES   ON  CACHE BOOL "" FORCE)
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(benchmark)
endif()

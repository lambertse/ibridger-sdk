cmake_minimum_required(VERSION 3.20)
project(iBridger VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(IBRIDGER_BUILD_SHARED     "Build shared library"          OFF)
option(IBRIDGER_BUILD_TESTS      "Build unit tests"              OFF)
option(IBRIDGER_BUILD_EXAMPLES   "Build example applications"    OFF)
option(IBRIDGER_BUILD_BENCHMARKS "Build performance benchmarks"  OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(IBRIDGER_BUILD_TESTS)
  enable_testing()
endif()

include(cmake/Dependencies.cmake)

# ─── Protobuf compilation ─────────────────────────────────────────────────────
# proto/ at repo root contains only .proto definitions (no toolchain logic).
# Each SDK owns its own compilation step; this is the C++ one.
if(DEFINED protobuf_SOURCE_DIR)
  include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)
endif()

set(PROTO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../proto)
set(PROTO_FILES
  ${PROTO_ROOT}/ibridger/envelope.proto
  ${PROTO_ROOT}/ibridger/rpc.proto
  ${PROTO_ROOT}/ibridger/constants.proto
  ${PROTO_ROOT}/ibridger/examples/echo.proto
)

add_library(ibridger_proto STATIC ${PROTO_FILES})
target_link_libraries(ibridger_proto PUBLIC protobuf::libprotobuf)
protobuf_generate(
  TARGET      ibridger_proto
  LANGUAGE    cpp
  IMPORT_DIRS ${PROTO_ROOT}
)
target_include_directories(ibridger_proto PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
)
if(NOT DEFINED protobuf_SOURCE_DIR)
  install(TARGETS ibridger_proto EXPORT iBridger-targets ARCHIVE DESTINATION lib)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ibridger
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
  )
endif()

add_library(ibridger::proto ALIAS ibridger_proto)

# ─── Core + SDK ───────────────────────────────────────────────────────────────
add_subdirectory(core)

if(IBRIDGER_BUILD_SHARED)
  set(LIB_TYPE SHARED)
else()
  set(LIB_TYPE STATIC)
endif()

add_library(ibridger_sdk_cpp ${LIB_TYPE}
  src/ibridger_sdk.cpp
  src/server_builder.cpp
  src/service_base.cpp
  src/client_stub.cpp
)

target_include_directories(ibridger_sdk_cpp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(ibridger_sdk_cpp PUBLIC ibridger::core)

add_library(ibridger::sdk::cpp ALIAS ibridger_sdk_cpp)

# ─── Install rules ────────────────────────────────────────────────────────────
install(TARGETS ibridger_sdk_cpp ARCHIVE DESTINATION lib)
install(DIRECTORY include/ibridger DESTINATION include)
if(NOT DEFINED protobuf_SOURCE_DIR)
  install(TARGETS ibridger_sdk_cpp EXPORT iBridger-targets)
endif()

# ─── CMake package config ─────────────────────────────────────────────────────
if(NOT DEFINED protobuf_SOURCE_DIR)
  include(CMakePackageConfigHelpers)

  install(EXPORT iBridger-targets
    FILE        iBridgerTargets.cmake
    NAMESPACE   ibridger::
    DESTINATION lib/cmake/iBridger
  )

  configure_package_config_file(
    cmake/iBridgerConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/iBridgerConfig.cmake
    INSTALL_DESTINATION lib/cmake/iBridger
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/iBridgerConfigVersion.cmake
    VERSION     ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/iBridgerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/iBridgerConfigVersion.cmake
    DESTINATION lib/cmake/iBridger
  )
endif()

# ─── Tests ────────────────────────────────────────────────────────────────────
if(IBRIDGER_BUILD_TESTS)
  add_executable(ibridger_sdk_tests tests/sdk_test.cpp)
  target_link_libraries(ibridger_sdk_tests PRIVATE ibridger::sdk::cpp GTest::gtest_main)
  gtest_discover_tests(ibridger_sdk_tests)
endif()

# ─── Examples ─────────────────────────────────────────────────────────────────
if(IBRIDGER_BUILD_EXAMPLES)
  add_executable(echo_server examples/server_example.cpp)
  target_include_directories(echo_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples)
  target_link_libraries(echo_server PRIVATE ibridger::sdk::cpp)

  add_executable(echo_client examples/client_example.cpp)
  target_link_libraries(echo_client PRIVATE ibridger::sdk::cpp)
endif()

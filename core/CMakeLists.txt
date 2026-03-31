add_library(ibridger_core STATIC
  src/ibridger.cpp
  src/common/logger.cpp
  src/common/error.cpp
  src/transport/unix_socket_transport.cpp
  src/transport/named_pipe_transport.cpp
  src/transport/transport_factory.cpp
  src/protocol/framing.cpp
  src/protocol/envelope_codec.cpp
  src/rpc/service_registry.cpp
  src/rpc/dispatcher.cpp
  src/rpc/server.cpp
  src/rpc/client.cpp
  src/rpc/builtin/ping_service.cpp
)

target_include_directories(ibridger_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(ibridger_core PUBLIC ibridger_proto)

# Namespaced alias so consumers can write target_link_libraries(... ibridger::core)
add_library(ibridger::core ALIAS ibridger_core)

# ─── Install rules ────────────────────────────────────────────────────────────
if(NOT DEFINED protobuf_SOURCE_DIR)
  install(TARGETS ibridger_core EXPORT iBridger-targets
    ARCHIVE DESTINATION lib
  )
  install(DIRECTORY include/ibridger
    DESTINATION include
  )
endif()

if(IBRIDGER_BUILD_TESTS)
  add_subdirectory(tests)
endif()

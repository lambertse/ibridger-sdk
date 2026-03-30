add_library(ibridger_core STATIC
  src/ibridger.cpp
  src/transport/unix_socket_transport.cpp
  src/transport/named_pipe_transport.cpp
  src/transport/transport_factory.cpp
  src/protocol/framing.cpp
  src/protocol/envelope_codec.cpp
  src/rpc/service_registry.cpp
  src/rpc/dispatcher.cpp
  src/rpc/server.cpp
)

target_include_directories(ibridger_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ibridger_core PUBLIC ibridger_proto)

if(IBRIDGER_BUILD_TESTS)
  add_subdirectory(tests)
endif()

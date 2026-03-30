add_executable(ibridger_tests
  sanity_test.cpp
  proto_test.cpp
  transport/unix_socket_test.cpp
  transport/named_pipe_test.cpp
  transport/transport_factory_test.cpp
  protocol/framing_test.cpp
  protocol/envelope_codec_test.cpp
  rpc/service_registry_test.cpp
  rpc/dispatcher_test.cpp
  rpc/server_test.cpp
)

target_link_libraries(ibridger_tests PRIVATE
  ibridger_core
  GTest::gtest_main
)

gtest_discover_tests(ibridger_tests)

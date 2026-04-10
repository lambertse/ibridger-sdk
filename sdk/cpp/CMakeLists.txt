option(IBRIDGER_BUILD_SHARED "Build shared library" OFF)
option(IBRIDGER_BUILD_TESTS "Build unit tests" OFF)

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

# Namespaced alias
add_library(ibridger::sdk::cpp ALIAS ibridger_sdk_cpp)

# ─── Install rules ────────────────────────────────────────────────────────────
install(TARGETS ibridger_sdk_cpp
  ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ibridger
  DESTINATION include
)
# FetchContent in-tree targets cannot be part of an export set; skip when protobuf
# was built from source.
if(NOT DEFINED protobuf_SOURCE_DIR)
  install(TARGETS ibridger_sdk_cpp EXPORT iBridger-targets)
endif()

# ─── Tests ────────────────────────────────────────────────────────────────────

if(IBRIDGER_BUILD_TESTS)
  add_executable(ibridger_sdk_tests
    tests/sdk_test.cpp
  )
  target_link_libraries(ibridger_sdk_tests PRIVATE
    ibridger::sdk::cpp
    GTest::gtest_main
  )
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

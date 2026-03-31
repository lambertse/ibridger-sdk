cmake_minimum_required(VERSION 3.20)
project(ibridger VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(IBRIDGER_BUILD_TESTS "Build unit tests" OFF)
option(IBRIDGER_BUILD_EXAMPLES "Build example applications" OFF)

if(IBRIDGER_BUILD_TESTS)
  enable_testing()
endif()

include(cmake/Dependencies.cmake)

add_subdirectory(proto)
add_subdirectory(core)
add_subdirectory(sdk/cpp)

# ─── CMake package install ────────────────────────────────────────────────────
# Skipped when protobuf was built via FetchContent (in-tree target cannot be
# part of an export set). cmake --install requires system protobuf.
if(NOT DEFINED protobuf_SOURCE_DIR)
  include(CMakePackageConfigHelpers)

  install(EXPORT ibridger-targets
    FILE        ibridgerTargets.cmake
    NAMESPACE   ibridger::
    DESTINATION lib/cmake/ibridger
  )

  configure_package_config_file(
    cmake/ibridgerConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ibridgerConfig.cmake
    INSTALL_DESTINATION lib/cmake/ibridger
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ibridgerConfigVersion.cmake
    VERSION     ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ibridgerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ibridgerConfigVersion.cmake
    DESTINATION lib/cmake/ibridger
  )
endif()

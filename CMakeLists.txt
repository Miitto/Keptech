cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech VERSION 0.1 LANGUAGES C CXX)

set(KT_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

find_program(SCCACHE_FOUND sccache)
include(ktx)

if(SCCACHE_FOUND)
  message(STATUS "Using sccache for compilation")
  set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_FOUND})
  set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_FOUND})

  # For MSVC debug information format
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
  cmake_policy(SET CMP0141 NEW)
else()
  message(STATUS "sccache not found, using default compiler")
endif()

option(KT_USE_PCH ON)
option(KT_BUILD_EXAMPLES "Build example applications" ON)
option(KT_ADD_RESOURCE_INFO "Add a name to resources. Allows for better debugging and for use in editors - but increaces memory usage." ON)

if(KT_BUILD_EXAMPLES)
  set(KT_ADD_RESOURCE_INFO ON)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  add_subdirectory(examples)
endif()

set(KT_PCH_HEADERS
  <algorithm>
  <expected>
  <optional>
  <functional>
  <memory>
  <string>
  <vector>
  <array>
  <unordered_map>
  <spdlog/spdlog.h>
  <bitset>
  <string>
  <string_view>
  <concepts>
  <set>
  <type_traits>
)

add_subdirectory(logging)
add_subdirectory(ecs)
add_subdirectory(core)
add_subdirectory(renderers)
add_subdirectory(engine)
add_subdirectory(shader_processor)
add_subdirectory(shader_embedder)

# Vendored Deps
add_subdirectory(vendor/imgui imgui)
add_subdirectory(vendor/stb stb)



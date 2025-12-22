cmake_minimum_required(VERSION 3.15..4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

option(BUILD_EXAMPLES "Build example applications" ON)
if (BUILD_EXAMPLES)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  add_subdirectory(examples)
endif()

project(Keptech VERSION 0.1 LANGUAGES CXX)

add_subdirectory(logging)
add_subdirectory(ecs)
add_subdirectory(core)
add_subdirectory(renderers)
add_subdirectory(engine)
add_subdirectory(shader_embedder)

# Vendored Deps
add_subdirectory(vendor/imgui imgui)



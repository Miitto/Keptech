cmake_minimum_required(VERSION 3.15..4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

project(Keptech VERSION 0.1 LANGUAGES CXX)

add_subdirectory(core)
add_subdirectory(engine)

# Vendored Deps
add_subdirectory(vendor/imgui imgui_cmake)

option(BUILD_EXAMPLES "Build example applications" ON)
if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

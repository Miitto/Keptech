cmake_minimum_required(VERSION 3.15..4.0)

project(imgui LANGUAGES CXX)

add_library(imgui)
add_library(imgui::imgui ALIAS imgui)

target_sources(imgui PRIVATE
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/backends/imgui_impl_vulkan.cpp
  imgui/backends/imgui_impl_sdl3.cpp
  PUBLIC
  FILE_SET HEADERS
  PUBLIC
  FILE_SET imGuiHeader
  TYPE HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/imgui
  FILES
  imgui/imgui.h
)

include(sdl)
link_sdl(imgui PUBLIC)

if (USE_VULKAN)
  include(vulkan)
  find_vulkan(23)
  link_vulkan(imgui PUBLIC)
endif()

# Disable warnings
target_compile_options(imgui PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-everything>
)

set_target_properties(imgui PROPERTIES
  CXX_CLANG_TIDY ""
)

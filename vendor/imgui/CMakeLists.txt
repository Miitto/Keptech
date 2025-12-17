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
  imgui/backends/imgui_impl_opengl3.cpp
  imgui/backends/imgui_impl_win32.cpp
  PUBLIC
  FILE_SET HEADERS
  PUBLIC
  FILE_SET imGuiHeader
  TYPE HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/imgui
  FILES
  imgui/imgui.h
)

# Disable warnings
target_compile_options(imgui PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-everything>
)

set_target_properties(imgui PROPERTIES
  CXX_CLANG_TIDY ""
)

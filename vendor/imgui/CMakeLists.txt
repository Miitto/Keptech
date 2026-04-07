cmake_minimum_required(VERSION 3.15..4.0)

project(ktimgui LANGUAGES CXX)

add_library(ktimgui)
add_library(imgui::imgui ALIAS ktimgui)

target_sources(ktimgui PRIVATE
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/backends/imgui_impl_sdl3.cpp
  imgui/misc/cpp/imgui_stdlib.cpp
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
link_sdl(ktimgui PUBLIC)

if (RENDERER STREQUAL "Vulkan")
  include(vulkan)
  find_vulkan(23)
  link_vulkan(ktimgui PUBLIC)
  target_sources(ktimgui PRIVATE
    imgui/backends/imgui_impl_vulkan.cpp
  )
endif()

# Disable warnings
target_compile_options(ktimgui PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-everything>
)

set_target_properties(ktimgui PROPERTIES
  CXX_CLANG_TIDY ""
)

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

target_compile_definitions(ktimgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)

if (RENDERER STREQUAL "Vulkan")
  link_vulkan(ktimgui PUBLIC)
  target_sources(ktimgui PRIVATE
    imgui/backends/imgui_impl_vulkan.cpp
  )
  target_compile_definitions(ktimgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK=1)
elseif (RENDERER STREQUAL "DX12")
  target_sources(ktimgui PRIVATE
    imgui/backends/imgui_impl_dx12.cpp
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

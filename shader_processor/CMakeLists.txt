cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_shader_processor)

add_library(keptech_shaders INTERFACE)
add_library(keptech::shaders ALIAS keptech_shaders)

target_sources(keptech_shaders
  INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
  FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/include/keptech/shaders/shader.h
)

include(spdlog)
link_spdlog(keptech_shaders INTERFACE)

project(Keptech_shader_processor)

add_library(keptech_shader_processor)
add_library(keptech::shader_processor ALIAS keptech_shader_processor)

include(slang)
link_slang(keptech_shader_processor PUBLIC)

link_spdlog(keptech_shader_processor PRIVATE)

target_sources(keptech_shader_processor
  PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    src/shader_processor.cpp
    src/printing.cpp
)

target_link_libraries(keptech_shader_processor
  PUBLIC
    keptech::shaders
)
link_spdlog(keptech_shader_processor PUBLIC)

target_compile_definitions(keptech_shader_processor
  PRIVATE
  KEPTECH_SHADER_DIR="${KT_SHADER_DIR}"
)

if (NOT PIPED_BUILD)
  target_compile_definitions(keptech_shader_processor PRIVATE USE_COLOR_OUTPUT=1)
endif()

if(KT_USE_PCH)
  target_precompile_headers(keptech_shader_processor PRIVATE <string> <string_view> <vector>)
endif()
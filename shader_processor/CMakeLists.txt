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

project(Keptech_shader_processor)

add_library(keptech_shader_processor)
add_library(keptech::shader_processor ALIAS keptech_shader_processor)

include(slang)
link_slang(keptech_shader_processor PUBLIC)

target_sources(keptech_shader_processor
  PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    src/shader_processor.cpp
)

target_link_libraries(keptech_shader_processor
  PUBLIC
    keptech::shaders
)

target_compile_definitions(keptech_shader_processor
  PRIVATE
  KEPTECH_SHADER_DIR="${KT_SHADER_DIR}"
)

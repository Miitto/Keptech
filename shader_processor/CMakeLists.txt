cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_shader_processor)

add_library(keptech_shaders)
add_library(keptech::shaders ALIAS keptech_shaders)
KT_SETUP_TARGET(keptech_shaders)

target_link_libraries(keptech_shaders
  PUBLIC
    keptech::logging
)

target_sources(keptech_shaders
  PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/keptech/shaders/dataType.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/keptech/shaders/fragment.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/keptech/shaders/info.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/keptech/shaders/resources.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/keptech/shaders/shader.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/keptech/shaders/stages.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/keptech/shaders/vertex.hpp
  PRIVATE
    keptech/shaders/formatting.cpp
)

project(Keptech_shader_processor)

add_library(keptech_shader_processor)
add_library(keptech::shader_processor ALIAS keptech_shader_processor)
KT_SETUP_TARGET(keptech_shader_processor)

link_slang(keptech_shader_processor PUBLIC)

target_sources(keptech_shader_processor
  PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
    keptech/shader_processor/shader_processor.cpp
    keptech/shader_processor/shader-logger.cpp
)

target_link_libraries(keptech_shader_processor
  PUBLIC
    keptech::logging
    keptech::shaders
    dxcompiler.lib
)

target_compile_definitions(keptech_shader_processor
  PRIVATE
  KEPTECH_SHADER_DIR="${KT_SHADER_DIR}"
  $<$<CONFIG:Debug>:SHADER_LOG_LEVEL=SPDLOG_LEVEL_${SHADER_DEBUG_LOG_LEVEL}>
  $<$<CONFIG:Release>:SHADER_LOG_LEVEL=SPDLOG_LEVEL_${SHADER_RELEASE_LOG_LEVEL}>
)

if (NOT PIPED_BUILD)
  target_compile_definitions(keptech_shader_processor PRIVATE USE_COLOR_OUTPUT=1)
endif()

if (RENDERER STREQUAL "Vulkan")
  target_compile_definitions(keptech_shaders INTERFACE KT_VULKAN=1)
elseif(RENDERER STREQUAL "DX12")
  target_compile_definitions(keptech_shaders INTERFACE KT_DX12=1)
endif()
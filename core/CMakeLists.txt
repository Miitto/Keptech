cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_Core LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::core ALIAS ${PROJECT_NAME})
KT_SETUP_TARGET(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::deps
    keptech::logging
    keptech::ecs
    keptech::shaders
)

target_sources(${PROJECT_NAME}
   PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(keptech)

set(KT_LOG_LEVEL "DEBUG" CACHE STRING "Core log level")

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    KT_LOG_LEVEL=SPDLOG_LEVEL_${KT_LOG_LEVEL}
    $<$<CXX_COMPILER_ID:GNU,Clang>:_KT_GNU>
    $<$<CXX_COMPILER_ID:MSVC>:_KT_MSVC>
)

if(KT_ADD_RESOURCE_INFO)
  target_compile_definitions(${PROJECT_NAME}
    PUBLIC
      KT_ADD_RESOURCE_INFO
  )
endif()

set(KT_PCH_HEADERS
  <functional>
  <memory>
  <vector>
  <array>
  <string>
  <string_view>
  <concepts>
  <type_traits>
  <cstdint>
  <spdlog/spdlog.h>
  <glm/glm.hpp>
)

if (RENDERER STREQUAL "Vulkan")
  list(APPEND KT_PCH_HEADERS
    <volk.h>
    <vk_mem_alloc.h>
  )
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE ${KT_PCH_HEADERS})
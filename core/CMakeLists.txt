cmake_minimum_required(VERSION 3.15..4.0)

add_library(Keptech_Core STATIC)
add_library(keptech::core ALIAS Keptech_Core)
KT_SETUP_TARGET(Keptech_Core)

target_link_libraries(Keptech_Core
  PUBLIC
    keptech::deps
    keptech::logging
    keptech::ecs
    keptech::shaders
)

target_sources(Keptech_Core
   PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(keptech)

set(KT_LOG_LEVEL "DEBUG" CACHE STRING "Core log level")

target_compile_definitions(Keptech_Core
  PUBLIC
    KT_LOG_LEVEL=SPDLOG_LEVEL_${KT_LOG_LEVEL}
    $<$<CXX_COMPILER_ID:GNU,Clang>:_KT_GNU>
    $<$<CXX_COMPILER_ID:MSVC>:_KT_MSVC>
)

if(KT_ADD_RESOURCE_INFO)
  target_compile_definitions(Keptech_Core
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
elseif(RENDERER STREQUAL "DX12")
  list(APPEND KT_PCH_HEADERS
    <d3d12.h>
    # <dxgi1_6.h>
    # <d3dcompiler.h>
  )
endif()

target_precompile_headers(Keptech_Core PRIVATE ${KT_PCH_HEADERS})
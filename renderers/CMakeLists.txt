set(RENDERER "Vulkan" CACHE STRING "The renderer to use")
set_property(CACHE RENDERER PROPERTY STRINGS "Vulkan;DX12")

add_library(Keptech_Renderer_Common STATIC)
add_library(keptech::renderer_common ALIAS Keptech_Renderer_Common)
KT_SETUP_TARGET(Keptech_Renderer_Common)

target_compile_definitions(Keptech_Renderer_Common
  PRIVATE
  $<$<CONFIG:Debug>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_DEBUG_LOG_LEVEL}>
  $<$<CONFIG:Release>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_RELEASE_LOG_LEVEL}>
)

target_link_libraries(Keptech_Renderer_Common
  PUBLIC
    keptech::core
)
target_precompile_headers(Keptech_Renderer_Common REUSE_FROM keptech::core)

add_subdirectory(common)

if(RENDERER STREQUAL "Vulkan")
  target_sources(Keptech_Renderer_Common
    PUBLIC
      FILE_SET vkHeaders
      TYPE HEADERS
      BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/vulkan
  )
  add_subdirectory(vulkan)
elseif(RENDERER STREQUAL "DX12")
  target_sources(Keptech_Renderer_Common
    PUBLIC
      FILE_SET dx12Headers
      TYPE HEADERS
      BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/dx12
  )
  add_subdirectory(dx12)
endif()

target_compile_definitions(Keptech_Renderer
  PRIVATE
  $<$<CONFIG:Debug>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_DEBUG_LOG_LEVEL}>
  $<$<CONFIG:Release>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_RELEASE_LOG_LEVEL}>
)
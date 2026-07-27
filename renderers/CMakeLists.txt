set(RENDERER "Vulkan" CACHE STRING "The renderer to use")
set_property(CACHE RENDERER PROPERTY STRINGS "Vulkan")

project(Keptech_Renderer_Common)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::renderer_common ALIAS ${PROJECT_NAME})
KT_SETUP_TARGET(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
  $<$<CONFIG:Debug>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_DEBUG_LOG_LEVEL}>
  $<$<CONFIG:Release>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_RELEASE_LOG_LEVEL}>
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::core
)
target_precompile_headers(${PROJECT_NAME} REUSE_FROM keptech::core)

add_subdirectory(common)

if(RENDERER STREQUAL "Vulkan")
  target_sources(${PROJECT_NAME}
    PUBLIC
      FILE_SET vkHeaders
      TYPE HEADERS
      BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/vulkan
  )
  target_compile_definitions(${PROJECT_NAME}
    PUBLIC
      KEPTECH_RENDERER_VULKAN=1
  )
  add_subdirectory(vulkan)
endif()

target_compile_definitions(Keptech_Renderer
  PRIVATE
  $<$<CONFIG:Debug>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_DEBUG_LOG_LEVEL}>
  $<$<CONFIG:Release>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_RELEASE_LOG_LEVEL}>
)
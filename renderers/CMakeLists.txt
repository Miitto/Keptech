set(RENDERER "Vulkan" CACHE STRING "The renderer to use")
set_property(CACHE RENDERER PROPERTY STRINGS "Vulkan")

project(Keptech_Renderer)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::renderer ALIAS ${PROJECT_NAME})

set(RENDERER_DEBUG_LOG_LEVEL DEBUG CACHE STRING "Set the log level for the rendererer")
set(RENDERER_RELEASE_LOG_LEVEL ERROR CACHE STRING "Set the log level for the rendererer in release mode")
set(RENDERER_LOG_LEVEL_OPTIONS "TRACE;DEBUG;INFO;WARN;ERROR;CRITICAL")
set_property(CACHE RENDERER_DEBUG_LOG_LEVEL PROPERTY STRINGS ${RENDERER_LOG_LEVEL_OPTIONS})
set_property(CACHE RENDERER_RELEASE_LOG_LEVEL PROPERTY STRINGS ${RENDERER_LOG_LEVEL_OPTIONS})

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
  $<$<CONFIG:Debug>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_DEBUG_LOG_LEVEL}>
  $<$<CONFIG:Release>:RENDERER_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_RELEASE_LOG_LEVEL}>
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::core
    imgui::imgui
)

include(mesh_optimizer)
link_mesh_optimizer(${PROJECT_NAME} PUBLIC)

add_subdirectory(common)

if(RENDERER STREQUAL "Vulkan")
  include(vulkan)
  find_vulkan(23)
  add_subdirectory(vulkan)
  message(STATUS "Vulkan renderer enabled")
endif()

if(KT_USE_PCH)
  target_precompile_headers(${PROJECT_NAME}
    PRIVATE
      ${KT_PCH_HEADERS}
  )
endif()
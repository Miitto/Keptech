set(RENDERER_LOG_LEVEL DEBUG CACHE STRING "Set the log level for the rendererer")
set(RENDERER_LOG_LEVEL_OPTIONS "TRACE;DEBUG;INFO;WARN;ERROR;CRITICAL")
set_property(CACHE RENDERER_LOG_LEVEL PROPERTY STRINGS ${RENDERER_LOG_LEVEL_OPTIONS})

add_executable(materials)

target_link_libraries(materials PRIVATE keptech::keptech keptech::shader_processor)

target_sources(materials
  PRIVATE
     FILE_SET HEADERS
  PRIVATE
    main.cpp
    editorLayer.cpp
)

target_compile_definitions(materials PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ASSET_DIR="${EXAMPLE_ASSET_DIR}")

add_subdirectory(shaders)

include(keptech_warnings)
KT_SETUP_WARNINGS(materials)

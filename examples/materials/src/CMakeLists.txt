set(RENDERERER_LOG_LEVEL DEBUG CACHE STRING "Set the log level for the rendererer")
set(RENDERERER_LOG_LEVEL_OPTIONS "TRACE;DEBUG;INFO;WARN;ERROR;CRITICAL")
set_property(CACHE RENDERERER_LOG_LEVEL PROPERTY STRINGS ${RENDERERER_LOG_LEVEL_OPTIONS})

add_executable(materials)

target_link_libraries(materials PRIVATE keptech::keptech)

target_sources(materials
  PRIVATE
     FILE_SET HEADERS
  PRIVATE
    main.cpp
)

target_compile_definitions(materials PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)

add_subdirectory(shaders)

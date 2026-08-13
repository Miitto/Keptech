add_executable(triangle_example)

target_link_libraries(triangle_example PRIVATE keptech::keptech)

KT_BINARY_TARGET(triangle_example)

target_sources(triangle_example
  PRIVATE
     FILE_SET HEADERS
  PRIVATE
    main.cpp
)

target_compile_definitions(triangle_example PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ASSET_DIR="${EXAMPLE_ASSET_DIR}")

compile_shader(triangle_example SPIRV
  NAMESPACE shaders
  BASE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_DIR
    examples/triangle
  LINK PRIVATE
  SOURCES
    triangle
)
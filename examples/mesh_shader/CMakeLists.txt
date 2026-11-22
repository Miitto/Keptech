add_executable(mesh_shader_example)
target_link_libraries(mesh_shader_example PRIVATE keptech::keptech)

KT_BINARY_TARGET(mesh_shader_example)

target_sources(mesh_shader_example
  PRIVATE
     FILE_SET HEADERS
  PRIVATE
    main.cpp
)

target_compile_definitions(mesh_shader_example PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ASSET_DIR="${EXAMPLE_ASSET_DIR}")


compile_shader(mesh_shader_example
  NAMESPACE shaders
  BASE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_DIR
    examples/mesh
  LINK PRIVATE
  SOURCES
    mesh
)
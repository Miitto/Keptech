cmake_minimum_required(VERSION 3.15..4.0)

target_sources(Keptech_Renderer_Common
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(keptech/render)

target_link_libraries(Keptech_Renderer_Common
  PUBLIC
    keptech::core
)

compile_shader(Keptech_Renderer_Common SPIRV
  NAMESPACE shaders
  BASE_DIR
    ${KT_SHADER_DIR}
  OUTPUT_DIR
    keptech
  LINK PUBLIC
  SOURCES
    geometry
)
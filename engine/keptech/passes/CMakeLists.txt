target_sources(Keptech
  PRIVATE
    data.cpp
    geometry.cpp
)

compile_shader(Keptech SPIRV
  NAMESPACE shaders::kt
  BASE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_DIR
    keptech
  LINK PRIVATE
  SOURCES
    geometry
    pointLight
)
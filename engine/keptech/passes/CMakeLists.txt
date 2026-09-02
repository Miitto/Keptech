target_sources(Keptech
  PRIVATE
    data.cpp
    debug.cpp
    geometry.cpp
    lightCombine.cpp
    lights.cpp
    skybox.cpp
)

compile_shader(Keptech
  NAMESPACE shaders::kt
  BASE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_DIR
    keptech
  LINK PRIVATE
  SOURCES
    geometry
    pointLight
    lightCombine
    skybox
)
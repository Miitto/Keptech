target_sources(Keptech
  PRIVATE
    colorGrade.cpp
    data.cpp
    debug.cpp
    fxaa.cpp
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
    colorGrade
    fxaa
    geometry
    pointLight
    lightCombine
    skybox
)
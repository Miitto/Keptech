target_sources(Keptech_RHI
  PRIVATE
    imgui.cpp
    pipelines.cpp
    setup.cpp
)

compile_shader(Keptech_RHI
  NAMESPACE shaders::kt
  BASE_DIR
    ${CMAKE_SOURCE_DIR}/shaders
  OUTPUT_DIR
    keptech/rhi
  LINK PRIVATE
  SOURCES
    blit
    cubeFromEquirectangular
    convoluteIrradiance
)
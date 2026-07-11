cmake_minimum_required(VERSION 3.15..4.0)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_subdirectory(src)

include(mesh_optimizer)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::core
    imgui::imgui
    kt::ktx
    meshoptimizer
)

include(shaders)

compile_shader(${PROJECT_NAME} SPIRV
  NAMESPACE shaders
  BASE_DIR
    ${KT_SHADER_DIR}
  OUTPUT_DIR
    keptech
  SOURCES
    basic
    mesh_shader
    pointLightShadows
    pointLight
    ssao
    ssaoBlur
    lightCombine
)

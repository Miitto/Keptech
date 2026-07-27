cmake_minimum_required(VERSION 3.15..4.0)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(keptech/rendering)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::core
)

include(shaders)

compile_shader(${PROJECT_NAME} SPIRV
  NAMESPACE shaders
  BASE_DIR
    ${KT_SHADER_DIR}
  OUTPUT_DIR
    keptech
  LINK PUBLIC
  SOURCES
    basic
    mesh_shader
    pointLightShadows
    pointLight
    ssao
    ssaoBlur
    lightCombine
)
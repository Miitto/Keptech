cmake_minimum_required(VERSION 3.15..4.0)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_subdirectory(src)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::core
    imgui::imgui
    kt::ktx
)

include(shaders)

compile_shader(${PROJECT_NAME} SPIRV
  BASE_DIR
    ${KT_SHADER_DIR}
  OUTPUT_DIR
    keptech
  SOURCES
    basic
    deferred
    pointLightShadows
    pointLight
    ssao
    ssaoBlur
    lightCombine
    downsample
    upsample
    bloomCombine
)

cmake_minimum_required(VERSION 3.15..4.0)

add_library(Keptech_Renderer STATIC)
add_library(keptech::renderer ALIAS Keptech_Renderer)
KT_SETUP_TARGET(Keptech_Renderer)

target_precompile_headers(Keptech_Renderer REUSE_FROM keptech::core)

target_link_libraries(Keptech_Renderer PUBLIC keptech::renderer_common )
target_link_libraries(Keptech_Renderer PRIVATE Keptech_Renderer_Common_shaders)

target_sources(Keptech_Renderer
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(keptech/render)
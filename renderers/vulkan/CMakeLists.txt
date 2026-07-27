cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_Renderer LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::renderer ALIAS ${PROJECT_NAME})
KT_SETUP_TARGET(${PROJECT_NAME})

target_precompile_headers(${PROJECT_NAME} REUSE_FROM keptech::core)

target_link_libraries(${PROJECT_NAME} PUBLIC keptech::renderer_common )
target_link_libraries(${PROJECT_NAME} PRIVATE Keptech_Renderer_Common_shaders)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(keptech/vulkan)

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    IMGUI_IMPL_VULKAN_USE_VOLK=1
)
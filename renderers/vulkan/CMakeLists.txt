cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_Renderer LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::renderer ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC keptech::renderer_common )
target_link_libraries(${PROJECT_NAME} PRIVATE Keptech_Renderer_Common_shaders)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(keptech/vulkan)

link_vulkan(${PROJECT_NAME} PUBLIC)

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    MAX_FRAMES_IN_FLIGHT=2
    IMGUI_IMPL_VULKAN_USE_VOLK=1
)

if(KT_USE_PCH)
  target_precompile_headers(${PROJECT_NAME}
    PRIVATE
      ${KT_PCH_HEADER}
      <Volk/volk.h>
      <vk_mem_alloc.h>
  )
endif()
cmake_minimum_required(VERSION 3.15..4.0)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(keptech/vulkan)

link_vulkan(${PROJECT_NAME} PUBLIC)

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    KEPTECH_RENDERER_VULKAN=1
    MAX_FRAMES_IN_FLIGHT=2
    IMGUI_IMPL_VULKAN_USE_VOLK=1
)

if(KT_USE_PCH)
  target_precompile_headers(${PROJECT_NAME}
    PRIVATE
      <Volk/volk.h>
      <vk_mem_alloc.h>
  )
endif()
cmake_minimum_required(VERSION 3.15..4.0)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_subdirectory(src)

link_vulkan(${PROJECT_NAME} PUBLIC)

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    KEPTECH_RENDERER_VULKAN=1
    KEPTECH_RENDERER_NAMESPACE=keptech::vkh
    MAX_FRAMES_IN_FLIGHT=2
    VK_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_LOG_LEVEL}
)

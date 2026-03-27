cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_Vulkan LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::vulkan ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
  PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

link_vulkan(${PROJECT_NAME} PUBLIC)
link_vma(${PROJECT_NAME} PUBLIC)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_subdirectory(src)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::core
    keptech::rendering
    imgui::imgui
)

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    KEPTECH_RENDERER_VULKAN=1
    KEPTECH_RENDERER_NAMESPACE=keptech::vkh
    MAX_FRAMES_IN_FLIGHT=2
    VK_LOG_LEVEL=SPDLOG_LEVEL_${RENDERER_LOG_LEVEL}
)

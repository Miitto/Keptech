cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_Rendering LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::rendering ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
  PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

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
)

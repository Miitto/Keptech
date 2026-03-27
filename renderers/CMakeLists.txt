set(RENDERER "Vulkan" CACHE STRING "The renderer to use")
set_property(CACHE RENDERER PROPERTY STRINGS "Vulkan")

project(Keptech_Renderer)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::renderer ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
  PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::core
    imgui::imgui
)

add_subdirectory(common)

if(RENDERER STREQUAL "Vulkan")
  include(vulkan)
  find_vulkan(23)
  add_subdirectory(vulkan)
  message(STATUS "Vulkan renderer enabled")
endif()

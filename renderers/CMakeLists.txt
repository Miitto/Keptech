set(RENDERER "Vulkan" CACHE STRING "The renderer to use")
set_property(CACHE RENDERER PROPERTY STRINGS "Vulkan")

add_subdirectory(common)

if(RENDERER STREQUAL "Vulkan")
  include(vulkan)
  find_vulkan(23)
  add_subdirectory(vulkan)
  message(STATUS "Vulkan renderer enabled")
endif()

set(KT_RENDERERS "Vulkan")
set(KEPTECH_RENDERER "Vulkan" CACHE STRING "Select the renderer to use")
set_property(CACHE KT_RENDERER PROPERTY STRINGS ${KT_RENDERERS})

if(NOT KEPTECH_RENDERER IN_LIST KT_RENDERERS)
  message(FATAL_ERROR "Keptech: Invalid renderer selected: ${KT_RENDERER}. Valid options are: ${KT_RENDERERS}")
endif()

message(STATUS "Keptech Renderer: ${KEPTECH_RENDERER}")
if(KEPTECH_RENDERER STREQUAL "Vulkan")
  include(vulkan)
  find_vulkan(23)
  add_subdirectory(vulkan)
endif()

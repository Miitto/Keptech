option(USE_VULKAN "Enable Vulkan renderer" ON)

if(USE_VULKAN)
  message(STATUS "Keptech: Vulkan Enabled")
  include(vulkan)
  find_vulkan(23)
  add_subdirectory(vulkan)
endif()

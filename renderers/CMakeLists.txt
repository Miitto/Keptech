
option(ENABLE_VULKAN "Enable Vulkan" ON)
if(ENABLE_VULKAN)
  include(vulkan)
  find_vulkan(23)
  add_subdirectory(vulkan)
endif()

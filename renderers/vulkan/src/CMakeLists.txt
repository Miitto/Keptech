target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET internalVulkanHeaders
    TYPE HEADERS
  PRIVATE
    setup/buffers.cpp
    setup/core.cpp
    setup/descriptors.cpp
    setup/device.cpp
    setup/imgui.cpp
    setup/pipelines.cpp
    setup/setup.cpp
    setup/swapchain.cpp

    helpers/conversions.cpp
    helpers/descriptors.cpp
    helpers/formatting.cpp
    helpers/instance.cpp
    helpers/physicalDevice.cpp
    helpers/queueFinder.cpp
    helpers/swapchain.cpp
    helpers/validators.cpp
    helpers/vmaImpl.cpp

    constants.cpp
    renderer.cpp
    resourceManagment.cpp
    structs.cpp
    vk-logger.cpp
    vmaImpl.cpp
)

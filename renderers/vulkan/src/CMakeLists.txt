target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
  PRIVATE
    setup/setup.cpp
    helpers/descriptors.cpp
    helpers/instance.cpp
    helpers/physicalDevice.cpp
    helpers/queueFinder.cpp
    helpers/shader.cpp
    helpers/swapchain.cpp
    helpers/validators.cpp
    helpers/vmaImpl.cpp

    renderer.cpp
    resourceManagment.cpp
    structs.cpp
    vk-logger.cpp
)

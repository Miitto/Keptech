target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
  PRIVATE
    setup/core.cpp
    helpers/descriptors.cpp
    helpers/dynLoader.cpp
    helpers/instance.cpp
    helpers/physicalDevice.cpp
    helpers/queueFinder.cpp
    helpers/shader.cpp
    helpers/swapchain.cpp
    helpers/validators.cpp
    helpers/vmaImpl.cpp

    mesh.cpp
    renderer.cpp
    rendering.cpp
    structs.cpp
    vk-logger.cpp
)

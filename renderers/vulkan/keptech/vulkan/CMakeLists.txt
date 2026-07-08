set(HELPER_HEADERS
    helpers/conversions.hpp
    helpers/formatting.hpp
    helpers/physicalDevice.hpp
    helpers/validators.hpp
    helpers/pipeline.hpp
)

set(HELPER_SOURCES
    helpers/conversions.cpp
    helpers/formatting.cpp
    helpers/physicalDevice.cpp
    helpers/validators.cpp
)

set(LOADING_HEADERS
    loading/mesh.hpp
)

set(LOADING_SOURCES
    loading/mesh.cpp
)

set(WRAPPER_SOURCES
    wrappers/buffer.cpp
    wrappers/image.cpp
    wrappers/instance.cpp
    wrappers/swapchain.cpp
)
set(WRAPPER_HEADERS
    wrappers/buffer.hpp
    wrappers/device.hpp
    wrappers/image.hpp
    wrappers/instance.hpp
    wrappers/swapchain.hpp
    wrappers/queue.hpp
)

set(SETUP_HEADERS
    setup/setup.hpp
)
set(SETUP_SOURCES
    setup/buffers.cpp
    setup/core.cpp
    setup/descriptors.cpp
    setup/device.cpp
    setup/formats.cpp
    setup/imgui.cpp
    setup/pipelines.cpp
    setup/samplers.cpp
    setup/setup.cpp
    setup/swapchain.cpp
    setup/targets.cpp
)

set(ALL_SOURCES
    ${HELPER_SOURCES}
    ${LOADING_SOURCES}
    ${WRAPPER_SOURCES}
    ${SETUP_SOURCES}
    impls.cpp
    constants.cpp
    impls.cpp
    renderer.cpp
    rendererConDes.cpp
    renderUtils.cpp
    resourceManagment.cpp
    structs.cpp
    vk-logger.cpp
)
set(ALL_HEADERS
    ${HELPER_HEADERS}
    ${LOADING_HEADERS}
    ${WRAPPER_HEADERS}
    ${SETUP_HEADERS}
    constants.hpp
    interface.hpp
    macros.hpp
    profile.hpp
    renderer.hpp
    structs.hpp
    vk-logger.hpp
)

target_sources(${PROJECT_NAME}
    PRIVATE
        FILE_SET internalHeaders
        TYPE HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            ${ALL_HEADERS}
    PRIVATE
        ${ALL_SOURCES}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Rendering/Vulkan/Headers" FILES ${ALL_HEADERS})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Rendering/Vulkan/Sources" FILES ${ALL_SOURCES})
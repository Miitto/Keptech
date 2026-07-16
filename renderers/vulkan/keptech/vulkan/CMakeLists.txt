set(HELPER_HEADERS
    helpers/conversions.hpp
    helpers/formatting.hpp
    helpers/physicalDevice.hpp
    helpers/pipeline.hpp
    helpers/transitions.hpp
    helpers/validators.hpp
    helpers/viewScissor.hpp
    helpers/owned.hpp
    helpers/transitions.hpp
)

set(HELPER_SOURCES
    helpers/conversions.cpp
    helpers/formatting.cpp
    helpers/physicalDevice.cpp
    helpers/pipeline.cpp
    helpers/validators.cpp
    helpers/viewScissor.cpp
    helpers/transitions.inl
)

set(LOADING_HEADERS
    loading/mesh.hpp
)

set(LOADING_SOURCES
    loading/mesh.cpp
)

set(PASSES_HEADERS
    passes/geometry.hpp
    passes/global.hpp
    passes/shadows.hpp
)

set(PASSES_SOURCES
    passes/geometry.cpp
    passes/global.cpp
    passes/shadows.cpp
)

set(RENDER_GRAPH_HEADERS
    renderGraph/builder.hpp
    renderGraph/graph.hpp
    renderGraph/pass.hpp
    renderGraph/renderResources.hpp
    renderGraph/types.hpp
)

set(RENDER_GRAPH_SOURCES
    renderGraph/builder.cpp
    renderGraph/pass.cpp
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

set(WRAPPER_SOURCES
    wrappers/buffer.cpp
    wrappers/image.cpp
    wrappers/instance.cpp
    wrappers/pipeline.cpp
    wrappers/swapchain.cpp
)
set(WRAPPER_HEADERS
    wrappers/buffer.hpp
    wrappers/image.hpp
    wrappers/instance.hpp
    wrappers/pipeline.hpp
    wrappers/swapchain.hpp
    wrappers/cmdBuf.hpp
    wrappers/device.hpp
    wrappers/fwd.hpp
    wrappers/queue.hpp
)

set(ALL_SOURCES
    ${HELPER_SOURCES}
    ${LOADING_SOURCES}
    ${PASSES_SOURCES}
    ${RENDER_GRAPH_SOURCES}
    ${WRAPPER_SOURCES}
    ${SETUP_SOURCES}
    constants.cpp
    core.cpp
    impls.cpp
    pipelines.cpp
    renderer.cpp
    rendererConDes.cpp
    renderUtils.cpp
    resourceManagment.cpp
    vk-logger.cpp
)
set(ALL_HEADERS
    ${HELPER_HEADERS}
    ${LOADING_HEADERS}
    ${PASSES_HEADERS}
    ${RENDER_GRAPH_HEADERS}
    ${WRAPPER_HEADERS}
    ${SETUP_HEADERS}
    constants.hpp
    core.hpp
    pipelines.hpp
    renderer.hpp
    vk-logger.hpp
    buffers.hpp
    gpuObjects.hpp
    interface.hpp
    macros.hpp
    profile.hpp
    structs.hpp
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
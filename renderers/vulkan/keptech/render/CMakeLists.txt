add_subdirectory(helpers)
add_subdirectory(loading)
add_subdirectory(passes)
add_subdirectory(setup)
add_subdirectory(wrappers)

target_sources(Keptech_Renderer
  PRIVATE
    constants.cpp
    renderer.cpp
    vk-logger.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Rendering/Vulkan")
add_subdirectory(helpers)
add_subdirectory(setup)
add_subdirectory(vk)

target_sources(Keptech_RHI
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
    buffer.cpp
    cmdBuf.cpp
    image.cpp
    rhi.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "RHI/Vulkan")
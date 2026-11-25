add_subdirectory(dx)
add_subdirectory(helpers)
add_subdirectory(setup)

target_sources(Keptech_RHI
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
    rhi.cpp
    buffer.cpp
    cmdBuf.cpp
    descriptorPool.cpp
    descriptorSet.cpp
    image.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "RHI/DX12")
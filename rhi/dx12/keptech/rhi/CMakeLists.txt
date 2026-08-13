add_subdirectory(helpers)
add_subdirectory(setup)
add_subdirectory(wrappers)

target_sources(Keptech_RHI
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
    constants.cpp
    dx-logger.cpp
    rhi.cpp
)

target_compile_definitions(Keptech_RHI
  PRIVATE
    WIN32_LEAN_AND_MEAN=1
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Rendering/DX12")
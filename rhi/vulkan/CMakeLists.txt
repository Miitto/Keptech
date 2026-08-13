target_sources(Keptech_RHI
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(keptech/rhi)
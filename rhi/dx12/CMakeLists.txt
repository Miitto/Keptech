target_sources(Keptech_RHI
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(Keptech_RHI
  SYSTEM
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

add_subdirectory(keptech/rhi)
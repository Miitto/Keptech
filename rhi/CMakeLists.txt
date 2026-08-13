cmake_minimum_required(VERSION 3.15..4.0)

add_library(Keptech_RHI STATIC)
add_library(keptech::rhi ALIAS Keptech_RHI)
KT_SETUP_TARGET(Keptech_RHI)

target_precompile_headers(Keptech_RHI REUSE_FROM keptech::core)

target_link_libraries(Keptech_RHI
  PUBLIC
    keptech::core
)

target_sources(Keptech_RHI
  PUBLIC
    FILE_SET commonHeaders
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/common
)
add_subdirectory(common/keptech/rhi)

if (RHI_BACKEND STREQUAL "Vulkan")
    add_subdirectory(vulkan)
elseif(RHI_BACKEND STREQUAL "DX12")
    add_subdirectory(dx12)
endif()

target_compile_definitions(Keptech_RHI
  PRIVATE
    RHI_LOG_LEVEL=SPDLOG_LEVEL_${RHI_LOG_LEVEL}
)
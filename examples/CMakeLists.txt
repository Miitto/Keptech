cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_Examples LANGUAGES CXX)

set(EXAMPLE_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/")

macro(KT_MAKE_EXAMPLE example_name)
    add_executable(${example_name})

    target_link_libraries(${example_name} PRIVATE keptech::keptech)

    KT_BINARY_TARGET(${example_name})

    target_sources(${example_name}
      PRIVATE
         FILE_SET HEADERS
    )

    target_compile_definitions(${example_name} PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ASSET_DIR="${EXAMPLE_ASSET_DIR}")
endmacro()

# add_subdirectory(basic)
add_subdirectory(clear_screen)
add_subdirectory(triangle)

if (RENDERER STREQUAL "Vulkan")
    add_subdirectory(mesh_shader)
endif()
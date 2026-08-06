cmake_minimum_required(VERSION 3.15..4.0)

add_library(Keptech)
add_library(keptech::keptech ALIAS Keptech)

KT_SETUP_TARGET(Keptech)

add_subdirectory(src)

target_sources(Keptech
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(Keptech
  PUBLIC
    keptech::core
    keptech::renderer
)

target_precompile_headers(Keptech REUSE_FROM keptech::core)

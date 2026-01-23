cmake_minimum_required(VERSION 3.10..4.0)

project(ktstb LANGUAGES C)

add_library(ktstb)
add_library(stb::stb ALIAS ktstb)

target_sources(ktstb
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    src/image.c
    src/truetype.c
)

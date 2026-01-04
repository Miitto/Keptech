cmake_minimum_required(VERSION 3.10..4.0)

project(stb LANGUAGES C)

add_library(stb)
add_library(stb::stb ALIAS stb)

target_sources(stb
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    src/image.c
    src/truetype.c
)

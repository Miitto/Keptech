cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_Logging VERSION 1.0.0 LANGUAGES CXX)

add_library(Keptech_Logging INTERFACE)
add_library(keptech::logging ALIAS Keptech_Logging)

target_sources(Keptech_Logging INTERFACE
  FILE_SET HEADERS
  BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)



cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_ESC LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::ecs ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    keptech::logging
)

# Lint

include(keptech_warnings)
KT_SETUP_WARNINGS(${PROJECT_NAME})

# Sourses

target_sources(${PROJECT_NAME}
   PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_subdirectory(src)

target_link_libraries(${PROJECT_NAME} PUBLIC keptech::core)

target_precompile_headers(${PROJECT_NAME} PUBLIC
  <string>
  <vector>
  <array>
  <unordered_map>
  <spdlog/spdlog.h>
  <optional>
  <functional>
  <bitset>
)

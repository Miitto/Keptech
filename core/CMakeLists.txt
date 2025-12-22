cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_Core LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::core ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Deps

include(glm)
link_glm(${PROJECT_NAME} PUBLIC)

include(spdlog)
link_spdlog(${PROJECT_NAME} PUBLIC)

include(sdl)
link_sdl(${PROJECT_NAME} PUBLIC)

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

target_precompile_headers(${PROJECT_NAME} PUBLIC
  <string>
  <vector>
  <array>
  <unordered_map>
  <concepts>
  <set>
  <type_traits>
  <spdlog/spdlog.h>
  <algorithm>
  <expected>
  <optional>
  <functional>
  <memory>
  <glm/glm.hpp>
)

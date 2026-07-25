cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech LANGUAGES CXX)

add_library(${PROJECT_NAME})
add_library(keptech::keptech ALIAS ${PROJECT_NAME})

KT_SETUP_TARGET(${PROJECT_NAME})

add_subdirectory(src)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::core
    keptech::renderer
)

if (KT_USE_PCH)
  target_precompile_headers(${PROJECT_NAME}
    PRIVATE
      <keptech/components.hpp>
      <keptech/keptech.hpp>
      <keptech/vulkan.hpp>
  )
endif()

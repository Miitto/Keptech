cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech LANGUAGES CXX)

add_library(${PROJECT_NAME})
add_library(keptech::keptech ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_subdirectory(src)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::core
)

if(USE_VULKAN)
  target_link_libraries(${PROJECT_NAME}
    PUBLIC
      keptech::vulkan
  )
endif()

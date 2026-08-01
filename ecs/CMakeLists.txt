cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_ESC LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::ecs ALIAS ${PROJECT_NAME})

KT_SETUP_TARGET(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::logging
    EnTT::EnTT
)

# Sourses
target_sources(${PROJECT_NAME}
   PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
   PRIVATE
    keptech/ecs/ecs-logger.cpp
)
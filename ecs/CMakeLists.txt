cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_ESC LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::ecs ALIAS ${PROJECT_NAME})

KT_SETUP_TARGET(${PROJECT_NAME})

# Deps
FetchContent_Declare(
  entt
  GIT_REPOSITORY https://github.com/skypjack/entt.git
  GIT_TAG v3.16.0
  SYSTEM
)
FetchContent_MakeAvailable(entt)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    keptech::logging
  PUBLIC
  EnTT::EnTT
)

# Sourses
target_sources(${PROJECT_NAME}
   PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_subdirectory(src)

target_link_libraries(${PROJECT_NAME} PUBLIC keptech::core)
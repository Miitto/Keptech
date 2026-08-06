cmake_minimum_required(VERSION 3.15..4.0)

add_library(Keptech_Ecs STATIC)
add_library(keptech::ecs ALIAS Keptech_Ecs)

KT_SETUP_TARGET(Keptech_Ecs)

target_link_libraries(Keptech_Ecs
  PUBLIC
    keptech::logging
    EnTT::EnTT
)

# Sourses
target_sources(Keptech_Ecs
   PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
   PRIVATE
    keptech/ecs/ecs-logger.cpp
)
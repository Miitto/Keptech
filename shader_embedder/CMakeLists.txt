cmake_minimum_required(VERSION 3.15..4.0)

add_executable(Keptech_shader_embedder main.cpp)

KT_BINARY_TARGET(Keptech_shader_embedder)

target_link_libraries(Keptech_shader_embedder
  PRIVATE
    keptech::shader_processor
)

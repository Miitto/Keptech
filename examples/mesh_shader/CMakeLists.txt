add_executable(monkey)
target_link_libraries(monkey PRIVATE keptech::keptech)

KT_BINARY_TARGET(monkey)

target_sources(monkey
  PRIVATE
     FILE_SET HEADERS
  PRIVATE
    main.cpp
)

target_compile_definitions(monkey PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ASSET_DIR="${EXAMPLE_ASSET_DIR}")


compile_shader(monkey SPIRV
  NAMESPACE shaders
  BASE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_DIR
    examples/monkey
  LINK PRIVATE
  SOURCES
    mesh
)
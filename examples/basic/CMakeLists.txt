add_executable(basic_example)

target_link_libraries(basic_example PRIVATE keptech::keptech)

KT_BINARY_TARGET(basic_example)

target_sources(basic_example
  PRIVATE
     FILE_SET HEADERS
  PRIVATE
    main.cpp
)

target_compile_definitions(basic_example PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ASSET_DIR="${EXAMPLE_ASSET_DIR}")
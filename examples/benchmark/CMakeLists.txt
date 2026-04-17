
add_executable(benchmark)

target_link_libraries(benchmark PRIVATE keptech::keptech keptech::shader_processor)

target_sources(benchmark
  PRIVATE
     FILE_SET HEADERS
  PRIVATE
    main.cpp
)

target_compile_definitions(benchmark PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ASSET_DIR="${EXAMPLE_ASSET_DIR}")

if (NOT SDL3_FOUND)
  add_custom_command(TARGET benchmark POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:benchmark> $<TARGET_RUNTIME_DLLS:benchmark>
    COMMAND_EXPAND_LISTS
  )
endif()

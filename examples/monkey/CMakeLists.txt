
add_executable(monkey)

target_link_libraries(monkey PRIVATE keptech::keptech keptech::shader_processor)

target_sources(monkey
  PRIVATE
     FILE_SET HEADERS
  PRIVATE
    main.cpp
)

target_compile_definitions(monkey PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ASSET_DIR="${EXAMPLE_ASSET_DIR}")

if (NOT SDL3_FOUND)
  add_custom_command(TARGET monkey POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:monkey> $<TARGET_RUNTIME_DLLS:monkey>
    COMMAND_EXPAND_LISTS
  )
endif()

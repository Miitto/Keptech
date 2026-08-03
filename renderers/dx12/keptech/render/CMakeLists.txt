add_subdirectory(setup)
add_subdirectory(renderer)
add_subdirectory(wrappers)

target_sources(${PROJECT_NAME}
    PRIVATE FILE_SET internalHeaders TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
    constants.cpp
    dx-logger.cpp
    graph.cpp
    renderer.cpp
)

target_compile_definitions(Keptech_Renderer
  PRIVATE
    WIN32_LEAN_AND_MEAN=1
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Rendering/DX12")
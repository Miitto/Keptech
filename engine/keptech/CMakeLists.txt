add_subdirectory(graph)
add_subdirectory(render)

target_sources(Keptech
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
  PRIVATE
    main.cpp
    input.cpp
)

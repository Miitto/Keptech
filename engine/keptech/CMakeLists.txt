add_subdirectory(gltf)
add_subdirectory(graph)
add_subdirectory(passes)
add_subdirectory(render)

target_sources(Keptech
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
  PRIVATE
    buffers.cpp
    imageFile.cpp
    input.cpp
    ktxImageFile.cpp
    main.cpp
    stbCubeImageFile.cpp
    stbImageFile.cpp
)

target_sources(Keptech_Renderer_Common
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
  PUBLIC
    gltf/data.cpp
    gltf/scene.cpp

    imgui.cpp
)

add_subdirectory(graph)
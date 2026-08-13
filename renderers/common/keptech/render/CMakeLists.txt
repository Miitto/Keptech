target_sources(Keptech_RHI_Common
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
  PUBLIC
    gltf/data.cpp
    gltf/scene.cpp

    imgui.cpp

    pipelineBuilder.cpp
)

add_subdirectory(graph)
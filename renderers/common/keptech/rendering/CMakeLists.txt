target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
  PUBLIC
    gltf/data.cpp
    gltf/scene.cpp

    interface/validate.cpp

    imgui.cpp
)

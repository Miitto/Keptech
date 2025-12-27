target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET private_headers
    TYPE HEADERS
  PRIVATE
    cameras/cameraManager.cpp
    components/transform.cpp
    gltf/loaded.cpp
    window.cpp
)

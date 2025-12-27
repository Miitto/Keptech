target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET private_headers
    TYPE HEADERS
  PRIVATE
    cameras/camera.cpp
    cameras/cameraManager.cpp
    components/transform.cpp
    gltf/loaded.cpp
    window.cpp
)

target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET private_headers
    TYPE HEADERS
  PRIVATE
    components/camera.cpp
    components/transform.cpp
    gltf/loaded.cpp
    layers/layerStack.cpp
    events.cpp
    gui.cpp
    kt-logger.cpp
    window.cpp
)

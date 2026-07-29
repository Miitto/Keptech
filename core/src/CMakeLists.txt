target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET private_headers
    TYPE HEADERS
  PRIVATE
    components/camera.cpp
    components/transform.cpp
    layers/layerStack.cpp
    maths/transform.cpp
    events.cpp
    fastgltf_formatting.cpp
    gui.cpp
    kt-logger.cpp
    scene.cpp
    window.cpp
)

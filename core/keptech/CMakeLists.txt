target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET private_headers
    TYPE HEADERS
  PRIVATE
    components/camera.cpp
    components/transform.cpp
    core/layers/layerStack.cpp
    maths/transform.cpp
    core/events/events.cpp
    core/fastgltf_formatting.cpp
    core/gui.cpp
    core/kt-logger.cpp
    core/scene.cpp
    core/window.cpp
)

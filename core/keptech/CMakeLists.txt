target_sources(Keptech_Core
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
    core/imgui.cpp
    core/kt-logger.cpp
    core/scene.cpp
    core/window.cpp
)

configure_file(
  core/version.h.in
  keptech/core/version.h
)

target_include_directories(Keptech_Core
  PUBLIC
    FILE_SET congfiguredHeaders
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${CMAKE_CURRENT_BINARY_DIR}/keptech/core/version.h
)
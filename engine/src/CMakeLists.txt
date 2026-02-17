target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET internalHeaders
    TYPE HEADERS
  PRIVATE
    main.cpp
    renderer.cpp
    rendering.cpp
    resourceManagment.cpp
)

include(shaders)

compile_shader(${PROJECT_NAME} SPIRV
  BASE_DIR
    ${KT_SHADER_DIR}
  OUTPUT_DIR
    keptech
  SOURCES
    deferred
    transparent
    pointLight
    lightCombine
)

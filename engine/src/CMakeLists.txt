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

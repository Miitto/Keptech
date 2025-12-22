target_sources(${PROJECT_NAME}
  PRIVATE
    FILE_SET private_headers
    TYPE HEADERS
  PRIVATE
    components/transform.cpp
    ecs/ecs-logger.cpp
    ecs/ecs.cpp
    window.cpp
)

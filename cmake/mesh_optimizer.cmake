FetchContent_Declare(
  mesh_optimizer
  GIT_REPOSITORY
    https://github.com/zeux/meshoptimizer.git
  GIT_TAG v1.1.1
  SYSTEM
)

FetchContent_MakeAvailable(mesh_optimizer)

function(link_mesh_optimizer TARGET_NAME ACCESS)
  target_link_libraries(${TARGET_NAME} ${ACCESS} meshoptimizer)
endfunction()


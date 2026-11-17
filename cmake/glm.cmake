FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.3 SYSTEM QUIET)
FetchContent_MakeAvailable(glm)

function(link_glm TARGET_NAME ACCESS)
  target_link_libraries(${TARGET_NAME} ${ACCESS} glm::glm)

endfunction()

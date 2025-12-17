FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG master)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

function(link_glfw TARGET_NAME ACCESS)
  target_link_libraries(${TARGET_NAME} ${ACCESS} glfw)
  target_compile_definitions(${TARGET_NAME} ${ACCESS} GLFW_INCLUDE_VULKAN)
endfunction()

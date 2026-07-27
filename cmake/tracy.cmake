  set(TRACY_ENABLE ON)
  message(STATUS "Profiling enabled, linking Tracy")
  FetchContent_Declare (
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.13.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SYSTEM
  )
  FetchContent_MakeAvailable(tracy)
else()
  message(STATUS "Profiling disabled")
endif()
find_package(SDL3 QUIET)

if (NOT SDL3_FOUND)
  message(STATUS "Downloading SDL3")
  # SDL logs a lot of stuff to the console, so we temporarily set the log level to WARNING to avoid cluttering the output
  cmake_language(GET_MESSAGE_LOG_LEVEL PRE_SDL_FETCH_LOG_LEVEL)
  set(CMAKE_MESSAGE_LOG_LEVEL "WARNING")
  FetchContent_Declare(
    sdl
    GIT_REPOSITORY
    https://github.com/libsdl-org/SDL
    GIT_TAG release-3.2.28
    SYSTEM
    QUIET
  )
  FetchContent_MakeAvailable(sdl)
  set(CMAKE_MESSAGE_LOG_LEVEL ${PRE_SDL_FETCH_LOG_LEVEL})
  set(SDL3_FOUND TRUE)
endif()

function(link_sdl TARGET_NAME ACCESS)
  target_link_libraries(${TARGET_NAME} ${ACCESS} SDL3::SDL3 SDL3::Headers)
endfunction()

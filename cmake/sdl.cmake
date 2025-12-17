FetchContent_Declare(
    sdl
    GIT_REPOSITORY
    https://github.com/libsdl-org/SDL
    GIT_TAG release-3.2.28
    SYSTEM
)
FetchContent_MakeAvailable(sdl)

function(link_sdl TARGET_NAME ACCESS)
  target_link_libraries(${TARGET_NAME} ${ACCESS} SDL3::SDL3)
endfunction()


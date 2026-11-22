set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "Disable exceptions in spdlog" FORCE)
if(WIN32)
  set(SPDLOG_WCHAR_SUPPORT ON CACHE BOOL "Enable wchar_t to UTF-8 conversion support" FORCE)
endif()

FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.15.3
  SYSTEM
)

FetchContent_MakeAvailable(spdlog)


function(link_spdlog target ACCESS)
  target_link_libraries(${target} ${ACCESS} spdlog::spdlog)
endfunction()

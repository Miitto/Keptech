find_package(slang QUIET)

if(NOT slang_FOUND)
  FetchContent_Declare(
    slang
    GIT_REPOSITORY https://github.com/shader-slang/slang.git
    GIT_TAG master
    SYSTEM
  )

  set(SLANG_ENABLE_TESTS OFF)
  set(SLANG_ENABLE_EXAMPLES OFF)
  set(SLANG_ENABLE_SLANGD OFF)
  set(SLANG_ENABLE_SLANGI OFF)
  set(SLANG_ENABLE_SLANG_RHI OFF)
  set(SLANG_ENABLE_SLANG_GLSLANG OFF)
  set(SLANG_ENABLE_GFX OFF)
  set(SLANG_ENABLE_CUDA OFF)
  set(SLANG_ENABLE_OPTIX OFF)
  FetchContent_MakeAvailable(slang)
  set_target_properties(slang PROPERTIES
    CXX_CLANG_TIDY ""
  )
  add_library(slang::slang ALIAS slang)
endif()

function(link_slang target ACCESS)
  target_link_libraries(${target} ${ACCESS} slang::slang)
endfunction()

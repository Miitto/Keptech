find_package(Ktx QUIET)



if (NOT Ktx_FOUND)
  message(STATUS "KTX library not found, building from source")
  FetchContent_Declare(
        fetch_ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software
        GIT_TAG        v5.0.0-rc2 # TODO: Shouldn't be using an RC, but earlier versions don't compile with C++ 23
        SYSTEM
        QUIET
)
  set(KTX_FEATURE_TESTS OFF)
  set(KTX_FEATURE_TOOLS OFF)
  FetchContent_MakeAvailable(fetch_ktx)

  add_library(ktktx INTERFACE)
  target_link_libraries(ktktx INTERFACE ktx)
  target_include_directories(ktktx SYSTEM INTERFACE ${fetch_ktx_SOURCE_DIR}/lib/include)
  target_include_directories(ktktx SYSTEM INTERFACE ${fetch_ktx_SOURCE_DIR}/external/dfdutils) # Needed for KHR/khr_df.h
  add_library(kt::ktx ALIAS ktktx)
else()
  message(STATUS "KTX library found, using system version")
  add_library(ktktx INTERFACE)
  target_link_libraries(ktktx INTERFACE KTX::ktx)
  add_library(kt::ktx ALIAS ktktx)
endif()

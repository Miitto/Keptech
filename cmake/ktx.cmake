find_package(Ktx QUIET)

if (NOT Ktx_FOUND)
  message(STATUS "KTX library not found, building from source")
  FetchContent_Declare(
        fetch_ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software
        GIT_TAG        v4.3.2
)
  FetchContent_MakeAvailable(fetch_ktx)
  add_library(ktktx INTERFACE)
  target_link_libraries(ktktx ktx)
  target_include_directories(ktktx INTERFACE ${fetch_ktx_SOURCE_DIR}/include)
  add_library(kt::ktx ALIAS ktktx)
else()
  message(STATUS "KTX library found, using system version")
  add_library(ktktx INTERFACE)
  target_link_libraries(ktktx INTERFACE KTX::ktx)
  add_library(kt::ktx ALIAS ktktx)
endif()

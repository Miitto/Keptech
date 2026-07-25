cmake_minimum_required(VERSION 3.24..4.0)

set(KT_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

find_program(Sccache sccache)

if(Sccache)
  message(STATUS "Using sccache for compilation")
  set(CMAKE_CXX_COMPILER_LAUNCHER ${Sccache})
  set(CMAKE_C_COMPILER_LAUNCHER ${Sccache})

  # For MSVC debug information format
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
  cmake_policy(SET CMP0141 NEW)
else()
  message(STATUS "sccache not found, using default compiler")
endif()

option(KT_USE_PCH ON)
option(KT_BUILD_EXAMPLES "Build example applications" OFF)
option(KT_ADD_RESOURCE_INFO "Add a name to resources. Allows for better debugging and for use in editors - but increaces memory usage." OFF)
option(KT_PROFILE "Enable profiling with Tracy" OFF)
option(PIPED_BUILD "Compiler output is piped, disabled colored output" OFF)
option(KT_TRACE_COMPILER "Create a trace of the compiler" OFF)

if (NOT PIPED_BUILD)
  message(STATUS "Enabling colored output for compiler diagnostics")
  list(APPEND CMAKE_C_FLAGS "-fdiagnostics-color=always -fansi-escape-codes")
  list(APPEND CMAKE_CXX_FLAGS "-fdiagnostics-color=always -fansi-escape-codes")
  set(CMAKE_COLOR_DIAGNOSTICS ON)
endif()

if (KT_PROFILE)
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

include(kt_fns)

if(KT_BUILD_EXAMPLES)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  add_compile_definitions(_LIBCPP_REMOVE_TRANSITIVE_INCLUDES)
endif()

project(Keptech VERSION 0.1 LANGUAGES C CXX)

if (CMAKE_CXX_STANDARD LESS 23)
  message(FATAL_ERROR "C++23 or higher is required")
endif()

include(ktx)

if(KT_BUILD_EXAMPLES)
  message(STATUS "Building examples")
  set(KT_ADD_RESOURCE_INFO ON)
  add_subdirectory(examples)
endif()

add_subdirectory(logging)
add_subdirectory(shader_processor)
add_subdirectory(shader_embedder)

add_subdirectory(ecs)
add_subdirectory(core)
add_subdirectory(renderers)
add_subdirectory(engine)


# Vendored Deps
add_subdirectory(vendor/imgui imgui)
add_subdirectory(vendor/stb stb)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
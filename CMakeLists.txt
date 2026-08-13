cmake_minimum_required(VERSION 3.24..4.0)

set(KT_VERSION_MAJOR 0)
set(KT_VERSION_MINOR 1)
set(KT_VERSION_PATCH 0)
set(KT_VERSION ${KT_VERSION_MAJOR}.${KT_VERSION_MINOR}.${KT_VERSION_PATCH})
set(KT_VERSION_STRING "${KT_VERSION_MAJOR}.${KT_VERSION_MINOR}.${KT_VERSION_PATCH}")

set(KT_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

include(kt_opts)
include(kt_fns)

if(KT_USE_SCCACHE)
  KT_ENABLE_SCCACHE()
endif()

if (NOT PIPED_BUILD)
  message(STATUS "Enabling colored output for compiler diagnostics")
  list(APPEND CMAKE_C_FLAGS "-fdiagnostics-color=always -fansi-escape-codes")
  list(APPEND CMAKE_CXX_FLAGS "-fdiagnostics-color=always -fansi-escape-codes")
  set(CMAKE_COLOR_DIAGNOSTICS ON)
endif()

if(KT_BUILD_EXAMPLES)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  add_compile_definitions(_LIBCPP_REMOVE_TRANSITIVE_INCLUDES)
  set(KT_ADD_RESOURCE_INFO ON)
endif()

project(Keptech VERSION ${KT_VERSION} LANGUAGES C CXX)

if (CMAKE_CXX_STANDARD LESS 23)
  message(FATAL_ERROR "C++23 or higher is required")
endif()

message(STATUS "RHI Backend: " ${RHI_BACKEND})

# Deps
include(kt_deps)
include(shaders)

add_subdirectory(logging)
add_subdirectory(shader_processor)
add_subdirectory(shader_embedder)

add_subdirectory(ecs)
add_subdirectory(core)
add_subdirectory(rhi)
add_subdirectory(engine)

if(KT_BUILD_EXAMPLES)
  message(STATUS "Building examples")
  add_subdirectory(examples)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
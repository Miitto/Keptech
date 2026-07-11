cmake_minimum_required(VERSION 3.15..4.0)

project(Keptech_Core LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(keptech::core ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Deps

include(glm)
link_glm(${PROJECT_NAME} PUBLIC)

include(spdlog)
link_spdlog(${PROJECT_NAME} PUBLIC)

include(sdl)
link_sdl(${PROJECT_NAME} PUBLIC)



target_link_libraries(${PROJECT_NAME}
  PUBLIC
    keptech::logging
    keptech::ecs
    keptech::shaders
    stb::stb
    imgui::imgui
)

FetchContent_Declare(
  fastgltf
  GIT_REPOSITORY
  https://github.com/spnda/fastgltf.git
  GIT_TAG
  main
  SYSTEM
)

FetchContent_MakeAvailable(fastgltf)
target_compile_options(fastgltf PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-everything>
)

set_target_properties(fastgltf PROPERTIES
  CXX_CLANG_TIDY ""
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    fastgltf::fastgltf
)

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
  target_link_libraries(${PROJECT_NAME}
    PUBLIC
    TracyClient
  )
  target_compile_definitions(${PROJECT_NAME}
    PUBLIC
    TRACY_ENABLE
    KT_PROFILE
  )
else()
  message(STATUS "Profiling disabled")
endif()

# Lint

include(keptech_warnings)
KT_SETUP_WARNINGS(${PROJECT_NAME})

# Sourses

target_sources(${PROJECT_NAME}
   PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_subdirectory(src)

if (KT_USE_PCH)
  target_precompile_headers(${PROJECT_NAME}
    PRIVATE
      ${KT_PCH_HEADERS}
      <glm/glm.hpp>
  )
endif()

set(KT_CORE_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

set(KT_LOG_LEVEL "DEBUG" CACHE STRING "Core log level")

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    KT_LOG_LEVEL=SPDLOG_LEVEL_${KT_LOG_LEVEL}
    $<$<CXX_COMPILER_ID:GNU,Clang>:_KT_GNU>
    $<$<CXX_COMPILER_ID:MSVC>:_KT_MSVC>
)

if(KT_ADD_RESOURCE_INFO)
  target_compile_definitions(${PROJECT_NAME}
    PUBLIC
      KT_ADD_RESOURCE_INFO
  )
endif()

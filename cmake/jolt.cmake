# When turning this option on, the library will be compiled using doubles for positions. This allows for much bigger worlds.
set(DOUBLE_PRECISION OFF)

# When turning this option on, the library will be compiled with debug symbols
if (NOT ${CMAKE_BUILD_TYPE} STREQUAL "Release")
  set(GENERATE_DEBUG_SYMBOLS ON)
endif ()

# When turning this option on, the library will override the default CMAKE_CXX_FLAGS_DEBUG/RELEASE values, otherwise they will use the platform defaults
set(OVERRIDE_CXX_FLAGS OFF)

# When turning this option on, the library will be compiled in such a way to attempt to keep the simulation deterministic across platforms
set(CROSS_PLATFORM_DETERMINISTIC ON)

# When turning this option on, the library will be compiled with interprocedural optimizations enabled, also known as link-time optimizations or link-time code generation.
# Note that if you turn this on you need to use SET_INTERPROCEDURAL_OPTIMIZATION() or set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON) to enable LTO specifically for your own project as well.
# If you don't do this you may get an error: /usr/bin/ld: libJolt.a: error adding symbols: file format not recognized
set(INTERPROCEDURAL_OPTIMIZATION ON)

# When turning this on, in Debug and Release mode, the library will emit extra code to ensure that the 4th component of a 3-vector is kept the same as the 3rd component
# and will enable floating point exceptions during simulation to detect divisions by zero.
# Note that this currently only works using MSVC. Clang turns Float2 into a SIMD vector sometimes causing floating point exceptions (the option is ignored).
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)

# When turning this on, the library will be compiled with C++ exceptions enabled.
# This adds some overhead and Jolt doesn't use exceptions so by default it is off.
set(CPP_EXCEPTIONS_ENABLED OFF)

# When turning this on, the library will be compiled with C++ RTTI enabled.
# This adds some overhead and Jolt doesn't use RTTI so by default it is off.
set(CPP_RTTI_ENABLED OFF)

# Number of bits to use in ObjectLayer. Can be 16 or 32.
set(OBJECT_LAYER_BITS 16)

# Select X86 processor features to use, by default the library compiles with AVX2, if everything is off it will be SSE2 compatible.
set(USE_SSE4_1 ON)
set(USE_SSE4_2 ON)
set(USE_AVX ON)
set(USE_AVX2 ON)
set(USE_AVX512 OFF)
set(USE_LZCNT ON)
set(USE_TZCNT ON)
set(USE_F16C ON)
set(USE_FMADD ON)

# Use dynamic runtime library to match rest of the project (MDd for debug, MD for release)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)

# Include Jolt
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY "https://github.com/jrouwe/JoltPhysics"
    GIT_TAG "v5.5.0"
    SOURCE_SUBDIR "Build"
    SYSTEM
)
FetchContent_MakeAvailable(JoltPhysics)

target_include_directories(Jolt PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/Physics>)

# Get compile and link options
get_target_property(JOLT_COMPILE_OPTIONS Jolt::Jolt COMPILE_OPTIONS)
get_target_property(JOLT_INTERFACE_COMPILE_OPTIONS Jolt::Jolt INTERFACE_COMPILE_OPTIONS)
get_target_property(JOLT_LINK_OPTIONS Jolt::Jolt LINK_OPTIONS)
get_target_property(JOLT_INTERFACE_LINK_OPTIONS Jolt::Jolt INTERFACE_LINK_OPTIONS)

# Remove -pthread
list(REMOVE_ITEM JOLT_COMPILE_OPTIONS "-pthread")
list(REMOVE_ITEM JOLT_INTERFACE_COMPILE_OPTIONS "-pthread")
list(REMOVE_ITEM JOLT_LINK_OPTIONS "-pthread")
list(REMOVE_ITEM JOLT_INTERFACE_LINK_OPTIONS "-pthread")

if (JOLT_COMPILE_OPTIONS STREQUAL "JOLT_COMPILE_OPTIONS-NOTFOUND")
  set(JOLT_COMPILE_OPTIONS "")
endif()

if(JOLT_INTERFACE_COMPILE_OPTIONS STREQUAL "JOLT_INTERFACE_COMPILE_OPTIONS-NOTFOUND")
  set(JOLT_INTERFACE_COMPILE_OPTIONS "")
endif()

if(JOLT_LINK_OPTIONS STREQUAL "JOLT_LINK_OPTIONS-NOTFOUND")
  set(JOLT_LINK_OPTIONS "")
endif()

if(JOLT_INTERFACE_LINK_OPTIONS STREQUAL "JOLT_INTERFACE_LINK_OPTIONS-NOTFOUND")
  set(JOLT_INTERFACE_LINK_OPTIONS "")
endif()

set_property(TARGET Jolt PROPERTY COMPILE_OPTIONS ${JOLT_COMPILE_OPTIONS})
set_property(TARGET Jolt PROPERTY INTERFACE_COMPILE_OPTIONS ${JOLT_INTERFACE_COMPILE_OPTIONS})
set_property(TARGET Jolt PROPERTY LINK_OPTIONS ${JOLT_LINK_OPTIONS})
set_property(TARGET Jolt PROPERTY INTERFACE_LINK_OPTIONS ${JOLT_INTERFACE_LINK_OPTIONS})

set_target_properties(Jolt PROPERTIES
  CXX_CLANG_TIDY ""
)

add_library(ktjolt INTERFACE)
add_library(kt::jolt ALIAS ktjolt)
target_link_libraries(ktjolt INTERFACE Jolt::Jolt)
target_include_directories(ktjolt INTERFACE ${JoltPhysics_SOURCE_DIR}/..)
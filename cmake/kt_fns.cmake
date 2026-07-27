set(KT_GNU_WARNINGS
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wsign-conversion
  -Wshadow
  -Wnon-virtual-dtor
  -Wcast-align
  -Woverloaded-virtual
  -Wnull-dereference
  -Wdouble-promotion
  -Wformat=2
  -Wno-c++17-extensions
  -Wno-format-security
  -Wno-old-style-cast
  -Wno-missing-designated-field-initializers
  -Wno-nullability-extension
)

set(KT_MSVC_WARNINGS
  /W4
  /permissive-
)

function(APPEND_TARGET_PROPERTY target property value)
  get_target_property(current_value ${target} ${property})
  if(NOT current_value)
    set(current_value "")
  endif()
  set(new_value "${current_value} ${value}")
  set_target_properties(${target} PROPERTIES ${property} "${new_value}")
endfunction()

function(KT_SETUP_WARNINGS TARGET)
  target_compile_options(${TARGET} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${KT_GNU_WARNINGS}>
    $<$<CXX_COMPILER_ID:MSVC>:${KT_MSVC_WARNINGS}>
  )
endfunction()

function(KT_SETUP_TARGET target)
    cmake_parse_arguments(PARSE_ARGV 0 arg "INTERFACE" "" "")
    if (NOT arg_INTERFACE)
        KT_SETUP_WARNINGS(${target})
    endif()
endfunction()

function(KT_ENABLE_SCCACHE)
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
endfunction()
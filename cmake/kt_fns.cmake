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
  -Wno-overriding-option
  -Wno-language-extension-token
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

function(KT_BINARY_TARGET target)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different -t $<TARGET_FILE_DIR:${target}> $<TARGET_RUNTIME_DLLS:${target}>
      COMMAND_EXPAND_LISTS
    )

    if (RHI_BACKEND STREQUAL "DX12")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${PIX_BIN_PATH}/WinPixEventRuntime.dll"
          $<TARGET_FILE_DIR:${target}>
      )
    endif()
endfunction()
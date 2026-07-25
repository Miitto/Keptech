set(KT_PCH_HEADERS
  <algorithm>
  <expected>
  <optional>
  <functional>
  <memory>
  <string>
  <vector>
  <array>
  <spdlog/spdlog.h>
  <string>
  <string_view>
  <concepts>
  <type_traits>
)

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
        if (KT_USE_PCH)
        target_precompile_headers(${target} PRIVATE ${KT_PCH_HEADERS})
        endif()
    endif()

    if (KT_PROFILE)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
            TracyClient
        )
        target_compile_definitions(${PROJECT_NAME}
            PUBLIC
            TRACY_ENABLE
            KT_PROFILE
        )
    endif()
endfunction()

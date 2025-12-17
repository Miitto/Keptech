set(KT_GNU_WARNINGS
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wsign-conversion
  -Wshadow
  -Wnon-virtual-dtor
  -Wold-style-cast
  -Wcast-align
  -Woverloaded-virtual
  -Wnull-dereference
  -Wdouble-promotion
  -Wformat=2
  -Wno-c++17-extensions
)

set(KT_MSVC_WARNINGS
  /W4
  /permissive-
)

function(KT_SETUP_WARNINGS TARGET)
  target_compile_options(${TARGET} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${KT_GNU_WARNINGS}>
    $<$<CXX_COMPILER_ID:MSVC>:${KT_MSVC_WARNINGS}>
  )
endfunction()

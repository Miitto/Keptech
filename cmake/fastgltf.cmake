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
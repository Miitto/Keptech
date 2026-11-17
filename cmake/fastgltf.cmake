FetchContent_Declare(
  fastgltf
  GIT_REPOSITORY
  https://github.com/spnda/fastgltf.git
  GIT_TAG
  v0.9.1 # TODO: Investigate getting v1 to work with clang. Linker error for `__extendhfsf2` due to attempting to use native 16 bit floats.
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
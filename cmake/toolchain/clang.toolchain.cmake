set(UNUSED ${CMAKE_TOOLCHAIN_FILE}) # Note: only to prevent cmake unused variable warninig

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

if(CLANG_TIDY_CHECKS)
  if (WIN32)
    find_program(COMPILER_PATH NAMES clang++.exe REQUIRED)

    cmake_path(GET COMPILER_PATH PARENT_PATH CLANG_PATH)

    set(CMAKE_CXX_CLANG_TIDY "${CLANG_PATH}/clang-tidy.exe;-checks=${CLANG_TIDY_CHECKS}")
  else()
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=${CLANG_TIDY_CHECKS}")
  endif()
endif()

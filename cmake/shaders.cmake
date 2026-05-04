find_program(SLANGC_EXECUTABLE NAMES slangc REQUIRED)

set(KT_SHADERS "${KT_SHADER_DIR}/lib/camera.slang" "${KT_SHADER_DIR}/lib/keptech.slang")

set(KT_SHADER_OPT_LEVELS "0" "1" "3")
set(KT_SHADER_OPT_LEVEL_DEBUG "0" CACHE STRING "Optimization level for shader compilation in Debug mode")
set(KT_SHADER_OPT_LEVEL_RELEASE "3" CACHE STRING "Optimization level for shader compilation in Release mode")

function(compile_shader target shader_target)
  set(SINGLEVALUE BASE_DIR OUTPUT_DIR)
  set(MULTIVALUE SOURCES INCLUDES)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "${SINGLEVALUE}" "${MULTIVALUE}")

  set(VALID_OUTPUT_TARGETS GLSL SPIRV)

  if(NOT shader_target IN_LIST VALID_OUTPUT_TARGETS)
    message(FATAL_ERROR "Invalid output target: ${shader_target}. Valid targets are: ${VALID_OUTPUT_TARGETS}")
  endif()

  if(arg_BASE_DIR)
    set(BASE_DIR ${arg_BASE_DIR})
  else()
    set(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  set(OUTPUTS "")
  set(INCLUDED_FILES "${KT_SHADERS}")

  foreach(file ${arg_INCLUDES})
    list(APPEND INCLUDED_FILES include/${file}.slang)
  endforeach()

  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

  set(OUT_HEADERS "")

  foreach(source ${arg_SOURCES})
    set(IN_FILE ${BASE_DIR}/${source}.slang)
    if (arg_OUTPUT_DIR)
      set(OUT_FILE ${CMAKE_BINARY_DIR}/shaders/gen/shaders/${arg_OUTPUT_DIR}/${source}.h)
    else()
      set(OUT_FILE ${CMAKE_BINARY_DIR}/shaders/gen/shaders/${source}.h)
    endif()

    add_custom_command(
      OUTPUT ${OUT_FILE}
      DEPENDS ${IN_FILE} ${INCLUDED_FILES} Keptech_shader_embedder
      COMMAND Keptech_shader_embedder ${source} ${IN_FILE} ${OUT_FILE}
      $<$<CONFIG:DEBUG>:${KT_SHADER_OPT_LEVEL_DEBUG}>$<$<CONFIG:RELWITHDEBINFO>:${KT_SHADER_OPT_LEVEL_RELEASE}>$<$<CONFIG:RELEASE>:${KT_SHADER_OPT_LEVEL_RELEASE}>
      $<$<CONFIG:DEBUG>:d>$<$<CONFIG:RELWITHDEBINFO>:d>
      COMMENT "Compiling shader [$<$<CONFIG:DEBUG>:${KT_SHADER_OPT_LEVEL_DEBUG}>$<$<CONFIG:RELWITHDEBINFO>:${KT_SHADER_OPT_LEVEL_RELEASE}>$<$<CONFIG:RELEASE>:${KT_SHADER_OPT_LEVEL_RELEASE}>] [$<$<CONFIG:DEBUG>:d>$<$<CONFIG:RELWITHDEBINFO>:d>]: ${IN_FILE} -> ${OUT_FILE}"
      VERBATIM
    )

    list(APPEND OUT_HEADERS ${OUT_FILE})
  endforeach()

  add_custom_target(${target}_shaders
    DEPENDS ${OUT_HEADERS}
    COMMENT "Embedding shaders into headers for target ${target}"
  )

  add_dependencies(${target} ${target}_shaders)

  target_include_directories(${target} PRIVATE ${CMAKE_BINARY_DIR}/shaders/gen)
endfunction()

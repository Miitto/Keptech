find_program(SLANGC_EXECUTABLE NAMES slangc REQUIRED)

set(KT_SHADERS "${KT_SHADER_DIR}/camera.slang" "${KT_SHADER_DIR}/keptech.slang")

function(compile_shader target shader_target)
  set(MULTIVALUE SOURCES INCLUDES)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "" "${MULTIVALUE}")

  set(VALID_OUTPUT_TARGETS GLSL SPIRV)

  if(NOT shader_target IN_LIST VALID_OUTPUT_TARGETS)
    message(FATAL_ERROR "Invalid output target: ${shader_target}. Valid targets are: ${VALID_OUTPUT_TARGETS}")
  endif()

  set(OUTPUTS "")
  set(INCLUDED_FILES "${KT_SHADERS}")

  foreach(file ${arg_INCLUDES})
    list(APPEND INCLUDED_FILES include/${file}.slang)
  endforeach()

  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

  set(OUT_HEADERS "")

  foreach(source ${arg_SOURCES})
    set(IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${source}.slang)
    set(OUT_FILE ${CMAKE_BINARY_DIR}/shaders/gen/shaders/${source}.h)

    add_custom_command(
    OUTPUT ${OUT_FILE}
    DEPENDS ${IN_FILE} ${KT_SHADERS} Keptech_shader_embedder
    COMMAND Keptech_shader_embedder ${source} ${IN_FILE} ${OUT_FILE}
    COMMENT "Embedding shaders into header for target ${target}"
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

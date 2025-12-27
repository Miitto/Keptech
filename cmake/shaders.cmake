find_program(SLANGC_EXECUTABLE NAMES slangc REQUIRED)

set(KT_SHADERS "${KT_SHADER_DIR}/camera.slang" "${KT_SHADER_DIR}/keptech.slang")

function(_compile_slang_file)
  set(SINGLEVALUE SOURCE OUT TARGET)
  set(MULTIVALUES ENTRIES INCLUDED_FILES)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "${SINGLEVALUE}" "${MULTIVALUES}")

  if(${arg_TARGET} STREQUAL GLSL)
    set(COMMAND_TARGET -target glsl -profile glsl_460)
  else()
    set(COMMAND_TARGET -target spirv -profile spirv_1_4 -emit-spirv-directly)
  endif()


  set(ENTRIES)
  foreach(entry ${arg_ENTRIES})
    set(ENTRIES ${ENTRIES} -entry ${entry})
  endforeach()
  add_custom_command(
    OUTPUT ${OUT_FILE}
    DEPENDS ${source}.slang ${arg_INCLUDED_FILES}
    COMMAND ${SLANGC_EXECUTABLE} ${arg_SOURCE} -I ${KT_SHADER_DIR} ${COMMAND_TARGET} -fvk-use-entrypoint-name ${ENTRIES} -o ${arg_OUT}
    COMMENT "Compiling shader ${source} to ${arg_TARGET} (${arg_OUT})"
    VERBATIM
  )
endfunction()


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
  foreach(source ${arg_SOURCES})
    set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${source}.slang)

    if(${shader_target} STREQUAL SPIRV)
      set(OUT_FILE ${CMAKE_BINARY_DIR}/shaders/raw/${source}.spv)
      _compile_slang_file(SOURCE ${SOURCE_FILE} TARGET ${shader_target} ENTRIES vert frag OUT ${OUT_FILE} INCLUDED_FILES ${INCLUDED_FILES})
      list(APPEND OUTPUTS ${OUT_FILE})
    else()
      foreach(stage vert frag)
        set(OUT_FILE ${CMAKE_BINARY_DIR}/shaders/raw/${source}_${stage}.glsl)
        _compile_slang_file(SOURCE ${SOURCE_FILE} TARGET ${shader_target} ENTRIES "${stage}" OUT ${OUT_FILE})
        list(APPEND OUTPUTS ${OUT_FILE})
      endforeach()
    endif()
  endforeach()

  add_custom_target(${target}_raw_shaders
    DEPENDS ${OUTPUTS}
    COMMENT "Compiling shaders for target ${target}"
  )

  set(OUT_HEADERS "")

  foreach(source ${arg_SOURCES})
    if(${shader_target} STREQUAL SPIRV)
      set(RAW_FILES ${CMAKE_BINARY_DIR}/shaders/raw/${source}.spv)
    else()
      set(RAW_FILES "")
      foreach(stage vert frag)
        set(RAW_FILES ${RAW_FILES} ${CMAKE_BINARY_DIR}/shaders/raw/${source}_${stage}.glsl)
      endforeach()
    endif()

    set(OUT_FILE ${CMAKE_BINARY_DIR}/shaders/gen/shaders/${source}.h)

    add_custom_command(
    OUTPUT ${OUT_FILE}
    DEPENDS ${RAW_FILES} Keptech_shader_embedder
    COMMAND Keptech_shader_embedder ${source} ${RAW_FILES} ${OUT_FILE}
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

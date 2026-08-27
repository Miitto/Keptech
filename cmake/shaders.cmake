find_program(SLANGC_EXECUTABLE NAMES slangc REQUIRED)

set(KT_SHADERS "${KT_SHADER_DIR}/lib/camera.slang" "${KT_SHADER_DIR}/lib/keptech.slang")

set(KT_SHADER_OPT_LEVELS "0" "1" "3")
set(KT_SHADER_OPT_LEVEL_DEBUG "0" CACHE STRING "Optimization level for shader compilation in Debug mode")
set(KT_SHADER_OPT_LEVEL_RELEASE "3" CACHE STRING "Optimization level for shader compilation in Release mode")

function(compile_shader target)
  set(SINGLEVALUE NAMESPACE BASE_DIR OUTPUT_DIR LINK)
  set(MULTIVALUE SOURCES INCLUDES)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "${SINGLEVALUE}" "${MULTIVALUE}")

  if(arg_BASE_DIR)
    set(BASE_DIR ${arg_BASE_DIR})
  else()
    set(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  set(INCLUDED_FILES "${KT_SHADERS}")

  foreach(file ${arg_INCLUDES})
    list(APPEND INCLUDED_FILES include/${file}.slang)
  endforeach()

  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

  set(OUT_HEADERS "")
  set(OUT_SOURCES "")

  foreach(source ${arg_SOURCES})
    set(IN_FILE ${BASE_DIR}/${source}.slang)
    if (arg_OUTPUT_DIR)
      set(OUT_HEADER ${CMAKE_BINARY_DIR}/shaders/gen/shaders/${arg_OUTPUT_DIR}/${source}.h)
      set(OUT_SOURCE ${CMAKE_BINARY_DIR}/shaders/gen/shaders/${arg_OUTPUT_DIR}/${source}.cpp)
    else()
      set(OUT_HEADER ${CMAKE_BINARY_DIR}/shaders/gen/shaders/${source}.h)
      set(OUT_SOURCE ${CMAKE_BINARY_DIR}/shaders/gen/shaders/${source}.cpp)
    endif()

    add_custom_command(
      OUTPUT ${OUT_HEADER} ${OUT_SOURCE}
      DEPENDS ${IN_FILE} ${INCLUDED_FILES} Keptech_shader_embedder
      COMMAND Keptech_shader_embedder ${source} ${arg_NAMESPACE} ${IN_FILE} ${OUT_HEADER} ${OUT_SOURCE}
      $<$<CONFIG:DEBUG>:${KT_SHADER_OPT_LEVEL_DEBUG}>$<$<CONFIG:RELWITHDEBINFO>:${KT_SHADER_OPT_LEVEL_RELEASE}>$<$<CONFIG:RELEASE>:${KT_SHADER_OPT_LEVEL_RELEASE}>
      $<$<CONFIG:DEBUG>:d>$<$<CONFIG:RELWITHDEBINFO>:d>
      COMMENT "Compiling shader [$<$<CONFIG:DEBUG>:${KT_SHADER_OPT_LEVEL_DEBUG}>$<$<CONFIG:RELWITHDEBINFO>:${KT_SHADER_OPT_LEVEL_RELEASE}>$<$<CONFIG:RELEASE>:${KT_SHADER_OPT_LEVEL_RELEASE}>] [$<$<CONFIG:DEBUG>:d>$<$<CONFIG:RELWITHDEBINFO>:d>]: ${IN_FILE} -> ${OUT_HEADER}"
      VERBATIM
    )

    list(APPEND OUT_HEADERS ${OUT_HEADER})
    list(APPEND OUT_SOURCES ${OUT_SOURCE})
  endforeach()

  set_source_files_properties(${OUT_HEADERS} ${OUT_SOURCES} PROPERTIES GENERATED TRUE)

  add_library(${target}_shaders)
  target_link_libraries(${target}_shaders PUBLIC keptech::shaders)

  target_sources(${target}_shaders PUBLIC FILE_SET HEADERS BASE_DIRS ${CMAKE_BINARY_DIR}/shaders/gen FILES ${OUT_HEADERS} PRIVATE ${OUT_SOURCES})

  if (NOT arg_LINK)
    set(arg_LINK PRIVATE)
  endif()

  target_link_libraries(${target} ${arg_LINK} ${target}_shaders)
endfunction()

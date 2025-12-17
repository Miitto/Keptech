function(link_imgui TARGET_NAME ACCESS)
  target_link_libraries(${TARGET_NAME} ${ACCESS} imgui::imgui)
endfunction()

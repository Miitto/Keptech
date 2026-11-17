message(CHECK_START "Checking for slang library")
find_package(slang QUIET)

if(NOT slang_FOUND)
  message(CHECK_FAIL "slang library not found")
else()
  message(CHECK_PASS "slang library found")
endif()

function(link_slang target ACCESS)
  target_link_libraries(${target} ${ACCESS} slang::slang)
endfunction()

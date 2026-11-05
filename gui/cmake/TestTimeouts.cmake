# Default test timeouts by label, so a local run and CI agree (R-302).
#
# ursatz_gui_apply_test_timeouts(<dir>) walks <dir> and its subdirectories and sets TIMEOUT 30 on every test without
# the `slow` label and TIMEOUT 1800 on every test with it. A test that already has a TIMEOUT keeps it. Call it once
# at the end of the top test CMakeLists.txt, after every test is registered.

# Reading and setting a test in another directory needs CMake 3.28; older versions run without default timeouts.
function(ursatz_gui_apply_test_timeouts dir)
  if(CMAKE_VERSION VERSION_LESS 3.28)
    message(STATUS "CMake ${CMAKE_VERSION} is older than 3.28: default test timeouts are not set")
    return()
  endif()
  get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(sub IN LISTS subdirs)
    ursatz_gui_apply_test_timeouts("${sub}")
  endforeach()

  get_property(tests DIRECTORY "${dir}" PROPERTY TESTS)
  foreach(name IN LISTS tests)
    get_property(existing TEST ${name} DIRECTORY "${dir}" PROPERTY TIMEOUT)
    if(existing)
      continue()
    endif()
    get_property(labels TEST ${name} DIRECTORY "${dir}" PROPERTY LABELS)
    if("slow" IN_LIST labels)
      set_tests_properties(${name} DIRECTORY "${dir}" PROPERTIES TIMEOUT 1800)
    else()
      set_tests_properties(${name} DIRECTORY "${dir}" PROPERTIES TIMEOUT 30)
    endif()
  endforeach()
endfunction()

# Default test timeouts by label, so local runs match CI (R-306).
# Tests without the `slow` label get 30 s, tests labeled `slow` get 1800 s.
# A test that already sets TIMEOUT keeps it.
#
# Call once at the end of the repo's top test CMakeLists.txt, after every test is registered.
# Needs CMake 3.28 (set_property(TEST ... DIRECTORY)); older versions skip with a notice.
function(ursatz_analyzer_apply_test_timeouts dir)
  if(CMAKE_VERSION VERSION_LESS 3.28)
    message(STATUS "ursatz_analyzer_apply_test_timeouts: CMake ${CMAKE_VERSION} < 3.28, default test timeouts not applied")
    return()
  endif()

  set(dirs "${dir}")
  get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  while(subdirs)
    list(POP_FRONT subdirs sub)
    list(APPEND dirs "${sub}")
    get_property(more DIRECTORY "${sub}" PROPERTY SUBDIRECTORIES)
    list(APPEND subdirs ${more})
  endwhile()

  foreach(d IN LISTS dirs)
    get_property(tests DIRECTORY "${d}" PROPERTY TESTS)
    foreach(t IN LISTS tests)
      get_property(has_timeout TEST "${t}" DIRECTORY "${d}" PROPERTY TIMEOUT SET)
      if(has_timeout)
        continue()
      endif()
      get_property(labels TEST "${t}" DIRECTORY "${d}" PROPERTY LABELS)
      if("slow" IN_LIST labels)
        set_property(TEST "${t}" DIRECTORY "${d}" PROPERTY TIMEOUT 1800)
      else()
        set_property(TEST "${t}" DIRECTORY "${d}" PROPERTY TIMEOUT 30)
      endif()
    endforeach()
  endforeach()
endfunction()

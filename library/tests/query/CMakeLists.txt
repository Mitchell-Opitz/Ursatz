add_ursatz_architecture_test(query)
add_ursatz_test(similarity_scoring)
add_ursatz_test(similarity_model)

# Scoped memcheck coverage for this module's own test binaries only (not a
# repo-wide valgrind mechanism -- see tests/generation/CMakeLists.txt for the
# same pattern). Skips silently if valgrind isn't installed.
find_program(URSATZ_QUERY_VALGRIND valgrind)
if(URSATZ_QUERY_VALGRIND)
  foreach(query_test_name similarity_model similarity_scoring)
    add_test(NAME ${query_test_name}_valgrind
             COMMAND ${URSATZ_QUERY_VALGRIND} --leak-check=full --error-exitcode=1
                     $<TARGET_FILE:test_${query_test_name}>)
  endforeach()
endif()

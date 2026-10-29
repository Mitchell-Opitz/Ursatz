add_ursatz_test(entity)
add_ursatz_test(value)
add_ursatz_test(occurrence)
add_ursatz_test(equality)
add_ursatz_test(references)
add_ursatz_test(string_copy)
add_ursatz_test(rational)
add_ursatz_test(once)
add_ursatz_architecture_test(core)

# Scoped memcheck coverage for this module's new helper tests. Skips silently if valgrind isn't installed.
find_program(URSATZ_CORE_VALGRIND valgrind)
if(URSATZ_CORE_VALGRIND)
  add_test(NAME string_copy_valgrind
           COMMAND ${URSATZ_CORE_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_string_copy>)
  add_test(NAME once_valgrind
           COMMAND ${URSATZ_CORE_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_once>)
  add_test(NAME rational_valgrind
           COMMAND ${URSATZ_CORE_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_rational>)
endif()

add_ursatz_test(persistence_lifecycle)
add_ursatz_test(persistence_note_events)
add_ursatz_test(persistence_uncertainty)

# Scoped memcheck coverage for this module's own test binaries only
# (not a repo-wide valgrind mechanism -- see docs/modules/io.md's own
# comment for the same pattern). Skips silently if valgrind isn't
# installed.
find_program(URSATZ_PERSISTENCE_VALGRIND valgrind)
if(URSATZ_PERSISTENCE_VALGRIND)
  foreach(persistence_test_name persistence_lifecycle persistence_note_events persistence_uncertainty)
    add_test(NAME ${persistence_test_name}_valgrind
             COMMAND ${URSATZ_PERSISTENCE_VALGRIND} --leak-check=full --error-exitcode=1
                     $<TARGET_FILE:test_${persistence_test_name}>)
  endforeach()
endif()

add_ursatz_test(persistence_lifecycle)
add_ursatz_test(persistence_note_events)
add_ursatz_test(persistence_uncertainty)

# persistence_chord_key needs real Chord/Key construction helpers from
# common-practice-minimal (frameworks/common-practice-minimal), not just
# ursatz -- add_ursatz_test only links ursatz, so this one is wired
# manually, the same way frameworks/common-practice-minimal/tests links
# its own tests against common_practice_minimal.
add_executable(test_persistence_chord_key test_persistence_chord_key.c)
target_link_libraries(test_persistence_chord_key PRIVATE common_practice_minimal)
add_test(NAME persistence_chord_key COMMAND test_persistence_chord_key)

add_ursatz_architecture_test(persistence)

# Scoped memcheck coverage for this module's own test binaries only
# (not a repo-wide valgrind mechanism -- see docs/modules/io.md's own
# comment for the same pattern). Skips silently if valgrind isn't
# installed.
find_program(URSATZ_PERSISTENCE_VALGRIND valgrind)
if(URSATZ_PERSISTENCE_VALGRIND)
  foreach(persistence_test_name persistence_lifecycle persistence_note_events persistence_uncertainty persistence_chord_key)
    add_test(NAME ${persistence_test_name}_valgrind
             COMMAND ${URSATZ_PERSISTENCE_VALGRIND} --leak-check=full --error-exitcode=1
                     $<TARGET_FILE:test_${persistence_test_name}>)
  endforeach()
endif()

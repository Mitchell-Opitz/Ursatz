add_ursatz_test(pitch_identity)
add_ursatz_test(pitch_spelling)
add_ursatz_test(tuning_system)
add_ursatz_test(pitch_class)
add_ursatz_test(pitch_realization)
add_ursatz_test(pitch_numeric_value)
add_ursatz_test(written_pitch)
add_ursatz_test(sounding_pitch)
add_ursatz_test(clef)
add_ursatz_test(staff_position)
add_ursatz_test(clef_resolution)
add_ursatz_test(accidental)
add_ursatz_test(key_signature)
add_ursatz_test(generic_interval)
add_ursatz_test(pitch_arithmetic)
add_ursatz_architecture_test(pitch)

# Scoped memcheck coverage for pitch arithmetic. Skips silently if valgrind isn't installed.
find_program(URSATZ_PITCH_VALGRIND valgrind)
if(URSATZ_PITCH_VALGRIND)
  add_test(NAME pitch_arithmetic_valgrind
           COMMAND ${URSATZ_PITCH_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_pitch_arithmetic>)
endif()

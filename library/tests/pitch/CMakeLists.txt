add_ursatz_test(pitch_identity)
add_ursatz_test(pitch_spelling)
add_ursatz_test(tuning_system)
add_ursatz_test(pitch_numeric_value)
add_ursatz_test(key_signature)
add_ursatz_test(pitch_arithmetic)
add_ursatz_test(midi_pitch)
add_ursatz_architecture_test(pitch)

# Scoped memcheck coverage for pitch arithmetic. Skips silently if valgrind isn't installed.
find_program(URSATZ_PITCH_VALGRIND valgrind)
if(URSATZ_PITCH_VALGRIND)
  add_test(NAME pitch_arithmetic_valgrind
           COMMAND ${URSATZ_PITCH_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_pitch_arithmetic>)
endif()

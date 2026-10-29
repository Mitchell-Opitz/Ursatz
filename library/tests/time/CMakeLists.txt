add_ursatz_test(duration)
add_ursatz_test(musical_time)
add_ursatz_test(time_span)
add_ursatz_test(time_instant)
add_ursatz_test(temporal_anchor)
add_ursatz_test(metronome_marking)
add_ursatz_test(historical_period)
add_ursatz_test(note_value)
add_ursatz_test(time_signature)
add_ursatz_test(meter_map)
add_ursatz_test(metrical_position)
add_ursatz_test(performance_time)
add_ursatz_test(tempo_map)
add_ursatz_test(tuplet)
add_ursatz_test(time_rational)
add_ursatz_architecture_test(time)

# Scoped memcheck coverage for the rational bridges. Skips silently if valgrind isn't installed.
find_program(URSATZ_TIME_VALGRIND valgrind)
if(URSATZ_TIME_VALGRIND)
  add_test(NAME time_rational_valgrind
           COMMAND ${URSATZ_TIME_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_time_rational>)
endif()

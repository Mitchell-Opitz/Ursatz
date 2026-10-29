add_ursatz_test(uncertainty)
add_ursatz_test(evidence)
add_ursatz_test(assertion)
add_ursatz_test(observation)
add_ursatz_test(hypothesis)
add_ursatz_test(interpretation)
add_ursatz_test(claim_interpretation)
add_ursatz_test(chord)
add_ursatz_test(key)
add_ursatz_test(scale_degree)
add_ursatz_test(texture)
add_ursatz_architecture_test(interpretation)

# Scoped memcheck coverage for this module's new helper tests. Skips silently if valgrind isn't installed.
find_program(URSATZ_INTERPRETATION_VALGRIND valgrind)
if(URSATZ_INTERPRETATION_VALGRIND)
  add_test(NAME claim_interpretation_valgrind
           COMMAND ${URSATZ_INTERPRETATION_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_claim_interpretation>)
endif()

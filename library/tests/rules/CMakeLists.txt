add_ursatz_test(rule)
add_ursatz_test(constraint)
add_ursatz_test(preference)
add_ursatz_test(preference_weight)
add_ursatz_test(preference_ranking)
add_ursatz_test(rule_evaluation)
add_ursatz_test(rule_pair)
add_ursatz_architecture_test(rules)

# Scoped memcheck coverage for this module's new helper tests. Skips silently if valgrind isn't installed.
find_program(URSATZ_RULES_VALGRIND valgrind)
if(URSATZ_RULES_VALGRIND)
  add_test(NAME rule_pair_valgrind
           COMMAND ${URSATZ_RULES_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_rule_pair>)
endif()

add_executable(test_uncertainty test_uncertainty.c)
target_link_libraries(test_uncertainty PRIVATE ursatz)
add_test(NAME uncertainty COMMAND test_uncertainty)

add_executable(test_evidence test_evidence.c)
target_link_libraries(test_evidence PRIVATE ursatz)
add_test(NAME evidence COMMAND test_evidence)

add_executable(test_assertion test_assertion.c)
target_link_libraries(test_assertion PRIVATE ursatz)
add_test(NAME assertion COMMAND test_assertion)

add_executable(test_observation test_observation.c)
target_link_libraries(test_observation PRIVATE ursatz)
add_test(NAME observation COMMAND test_observation)

add_executable(test_hypothesis test_hypothesis.c)
target_link_libraries(test_hypothesis PRIVATE ursatz)
add_test(NAME hypothesis COMMAND test_hypothesis)

add_executable(test_interpretation test_interpretation.c)
target_link_libraries(test_interpretation PRIVATE ursatz)
add_test(NAME interpretation COMMAND test_interpretation)

add_executable(test_architecture_interpretation test_architecture_interpretation.c)
add_test(NAME architecture_interpretation COMMAND test_architecture_interpretation
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_uncertainty test_uncertainty.c)
target_link_libraries(test_uncertainty PRIVATE ursatz)
add_test(NAME uncertainty COMMAND test_uncertainty)

add_executable(test_evidence test_evidence.c)
target_link_libraries(test_evidence PRIVATE ursatz)
add_test(NAME evidence COMMAND test_evidence)

add_executable(test_architecture_interpretation test_architecture_interpretation.c)
add_test(NAME architecture_interpretation COMMAND test_architecture_interpretation
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
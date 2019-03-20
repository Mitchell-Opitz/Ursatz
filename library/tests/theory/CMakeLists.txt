add_executable(test_framework test_framework.c)
target_link_libraries(test_framework PRIVATE ursatz)
add_test(NAME framework COMMAND test_framework)

add_executable(test_heuristic test_heuristic.c)
target_link_libraries(test_heuristic PRIVATE ursatz)
add_test(NAME heuristic COMMAND test_heuristic)

add_executable(test_tendency test_tendency.c)
target_link_libraries(test_tendency PRIVATE ursatz)
add_test(NAME tendency COMMAND test_tendency)

add_executable(test_statistical_model test_statistical_model.c)
target_link_libraries(test_statistical_model PRIVATE ursatz)
add_test(NAME statistical_model COMMAND test_statistical_model)

add_executable(test_architecture_theory test_architecture_theory.c)
add_test(NAME architecture_theory COMMAND test_architecture_theory
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
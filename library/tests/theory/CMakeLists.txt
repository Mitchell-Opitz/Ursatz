add_executable(test_architecture_theory test_architecture_theory.c)
add_test(NAME architecture_theory COMMAND test_architecture_theory
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
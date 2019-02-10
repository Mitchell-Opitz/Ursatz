add_executable(test_architecture_interpretation test_architecture_interpretation.c)
add_test(NAME architecture_interpretation COMMAND test_architecture_interpretation
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_architecture_rules test_architecture_rules.c)
add_test(NAME architecture_rules COMMAND test_architecture_rules
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
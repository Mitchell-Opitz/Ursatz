add_executable(test_architecture_context test_architecture_context.c)
add_test(NAME architecture_context COMMAND test_architecture_context
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
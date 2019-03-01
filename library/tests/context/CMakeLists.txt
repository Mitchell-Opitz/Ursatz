add_executable(test_context test_context.c)
target_link_libraries(test_context PRIVATE ursatz)
add_test(NAME context COMMAND test_context)

add_executable(test_architecture_context test_architecture_context.c)
add_test(NAME architecture_context COMMAND test_architecture_context
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
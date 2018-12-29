add_executable(test_architecture_events test_architecture_events.c)
add_test(NAME architecture_events COMMAND test_architecture_events
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
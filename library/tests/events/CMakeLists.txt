add_executable(test_event test_event.c)
target_link_libraries(test_event PRIVATE ursatz)
add_test(NAME event COMMAND test_event)

add_executable(test_architecture_events test_architecture_events.c)
add_test(NAME architecture_events COMMAND test_architecture_events
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
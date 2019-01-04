add_executable(test_event test_event.c)
target_link_libraries(test_event PRIVATE ursatz)
add_test(NAME event COMMAND test_event)

add_executable(test_note_event test_note_event.c)
target_link_libraries(test_note_event PRIVATE ursatz)
add_test(NAME note_event COMMAND test_note_event)

add_executable(test_rest_event test_rest_event.c)
target_link_libraries(test_rest_event PRIVATE ursatz)
add_test(NAME rest_event COMMAND test_rest_event)

add_executable(test_architecture_events test_architecture_events.c)
add_test(NAME architecture_events COMMAND test_architecture_events
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
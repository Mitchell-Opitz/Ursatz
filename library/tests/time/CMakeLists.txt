add_executable(test_duration test_duration.c)
target_link_libraries(test_duration PRIVATE ursatz)
add_test(NAME duration COMMAND test_duration)

add_executable(test_musical_time test_musical_time.c)
target_link_libraries(test_musical_time PRIVATE ursatz)
add_test(NAME musical_time COMMAND test_musical_time)

add_executable(test_architecture_time test_architecture_time.c)
add_test(NAME architecture_time COMMAND test_architecture_time
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
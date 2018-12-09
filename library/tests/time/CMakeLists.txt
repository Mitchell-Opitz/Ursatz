add_executable(test_architecture_time test_architecture_time.c)
add_test(NAME architecture_time COMMAND test_architecture_time
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
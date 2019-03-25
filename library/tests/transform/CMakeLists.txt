add_executable(test_architecture_transform test_architecture_transform.c)
add_test(NAME architecture_transform COMMAND test_architecture_transform
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
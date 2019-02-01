add_executable(test_architecture_relationship test_architecture_relationship.c)
add_test(NAME architecture_relationship COMMAND test_architecture_relationship
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
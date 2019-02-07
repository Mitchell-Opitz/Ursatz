add_executable(test_relationship test_relationship.c)
target_link_libraries(test_relationship PRIVATE ursatz)
add_test(NAME relationship COMMAND test_relationship)

add_executable(test_architecture_relationship test_architecture_relationship.c)
add_test(NAME architecture_relationship COMMAND test_architecture_relationship
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_architecture_containers test_architecture_containers.c)
add_test(NAME architecture_containers COMMAND test_architecture_containers
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
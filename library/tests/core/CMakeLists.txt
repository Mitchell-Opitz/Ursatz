add_executable(test_architecture_core test_architecture_core.c)
add_test(NAME architecture_core COMMAND test_architecture_core
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
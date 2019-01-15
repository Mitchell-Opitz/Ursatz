add_executable(test_architecture_structure test_architecture_structure.c)
add_test(NAME architecture_structure COMMAND test_architecture_structure
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
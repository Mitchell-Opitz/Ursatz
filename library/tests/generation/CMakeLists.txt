add_executable(test_architecture_generation test_architecture_generation.c)
add_test(NAME architecture_generation COMMAND test_architecture_generation
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
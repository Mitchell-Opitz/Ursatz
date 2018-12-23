add_executable(test_architecture_pitch test_architecture_pitch.c)
add_test(NAME architecture_pitch COMMAND test_architecture_pitch
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
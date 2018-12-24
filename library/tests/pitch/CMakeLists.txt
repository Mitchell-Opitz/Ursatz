add_executable(test_pitch_identity test_pitch_identity.c)
target_link_libraries(test_pitch_identity PRIVATE ursatz)
add_test(NAME pitch_identity COMMAND test_pitch_identity)

add_executable(test_architecture_pitch test_architecture_pitch.c)
add_test(NAME architecture_pitch COMMAND test_architecture_pitch
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
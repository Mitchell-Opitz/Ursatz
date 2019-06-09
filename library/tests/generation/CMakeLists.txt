add_executable(test_intent test_intent.c)
target_link_libraries(test_intent PRIVATE ursatz)
add_test(NAME intent COMMAND test_intent)

add_executable(test_architecture_generation test_architecture_generation.c)
add_test(NAME architecture_generation COMMAND test_architecture_generation
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
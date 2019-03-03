add_executable(test_framework test_framework.c)
target_link_libraries(test_framework PRIVATE ursatz)
add_test(NAME framework COMMAND test_framework)

add_executable(test_architecture_theory test_architecture_theory.c)
add_test(NAME architecture_theory COMMAND test_architecture_theory
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
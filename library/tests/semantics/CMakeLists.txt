add_executable(test_interval test_interval.c)
target_link_libraries(test_interval PRIVATE ursatz)
add_test(NAME interval COMMAND test_interval)

add_executable(test_architecture_semantics test_architecture_semantics.c)
add_test(NAME architecture_semantics COMMAND test_architecture_semantics
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
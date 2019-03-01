add_executable(test_interval test_interval.c)
target_link_libraries(test_interval PRIVATE ursatz)
add_test(NAME interval COMMAND test_interval)

add_executable(test_sonority test_sonority.c)
target_link_libraries(test_sonority PRIVATE ursatz)
add_test(NAME sonority COMMAND test_sonority)

add_executable(test_rhythmic_pattern test_rhythmic_pattern.c)
target_link_libraries(test_rhythmic_pattern PRIVATE ursatz)
add_test(NAME rhythmic_pattern COMMAND test_rhythmic_pattern)

add_executable(test_architecture_semantics test_architecture_semantics.c)
add_test(NAME architecture_semantics COMMAND test_architecture_semantics
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
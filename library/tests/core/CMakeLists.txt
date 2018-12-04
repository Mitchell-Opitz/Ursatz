add_executable(test_entity test_entity.c)
target_link_libraries(test_entity PRIVATE ursatz)
add_test(NAME entity COMMAND test_entity)

add_executable(test_value test_value.c)
target_link_libraries(test_value PRIVATE ursatz)
add_test(NAME value COMMAND test_value)

add_executable(test_occurrence test_occurrence.c)
target_link_libraries(test_occurrence PRIVATE ursatz)
add_test(NAME occurrence COMMAND test_occurrence)

add_executable(test_equality test_equality.c)
target_link_libraries(test_equality PRIVATE ursatz)
add_test(NAME equality COMMAND test_equality)

add_executable(test_architecture_core test_architecture_core.c)
add_test(NAME architecture_core COMMAND test_architecture_core
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_entity test_entity.c)
target_link_libraries(test_entity PRIVATE ursatz)
add_test(NAME entity COMMAND test_entity)

add_executable(test_architecture_core test_architecture_core.c)
add_test(NAME architecture_core COMMAND test_architecture_core
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
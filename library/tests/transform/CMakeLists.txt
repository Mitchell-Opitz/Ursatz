add_executable(test_transformation test_transformation.c)
target_link_libraries(test_transformation PRIVATE ursatz)
add_test(NAME transformation COMMAND test_transformation)

add_executable(test_architecture_transform test_architecture_transform.c)
add_test(NAME architecture_transform COMMAND test_architecture_transform
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
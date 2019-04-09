add_executable(test_transformation test_transformation.c)
target_link_libraries(test_transformation PRIVATE ursatz)
add_test(NAME transformation COMMAND test_transformation)

add_executable(test_transposition test_transposition.c)
target_link_libraries(test_transposition PRIVATE ursatz)
add_test(NAME transposition COMMAND test_transposition)

add_executable(test_inversion test_inversion.c)
target_link_libraries(test_inversion PRIVATE ursatz)
add_test(NAME inversion COMMAND test_inversion)

add_executable(test_retrograde test_retrograde.c)
target_link_libraries(test_retrograde PRIVATE ursatz)
add_test(NAME retrograde COMMAND test_retrograde)

add_executable(test_retrograde_inversion test_retrograde_inversion.c)
target_link_libraries(test_retrograde_inversion PRIVATE ursatz)
add_test(NAME retrograde_inversion COMMAND test_retrograde_inversion)

add_executable(test_architecture_transform test_architecture_transform.c)
add_test(NAME architecture_transform COMMAND test_architecture_transform
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
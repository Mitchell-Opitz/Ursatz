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

add_executable(test_augmentation test_augmentation.c)
target_link_libraries(test_augmentation PRIVATE ursatz)
add_test(NAME augmentation COMMAND test_augmentation)

add_executable(test_diminution test_diminution.c)
target_link_libraries(test_diminution PRIVATE ursatz)
add_test(NAME diminution COMMAND test_diminution)

add_executable(test_fragmentation test_fragmentation.c)
target_link_libraries(test_fragmentation PRIVATE ursatz)
add_test(NAME fragmentation COMMAND test_fragmentation)

add_executable(test_expansion test_expansion.c)
target_link_libraries(test_expansion PRIVATE ursatz)
add_test(NAME expansion COMMAND test_expansion)

add_executable(test_contraction test_contraction.c)
target_link_libraries(test_contraction PRIVATE ursatz)
add_test(NAME contraction COMMAND test_contraction)

add_executable(test_displacement test_displacement.c)
target_link_libraries(test_displacement PRIVATE ursatz)
add_test(NAME displacement COMMAND test_displacement)

add_executable(test_ornamentation test_ornamentation.c)
target_link_libraries(test_ornamentation PRIVATE ursatz)
add_test(NAME ornamentation COMMAND test_ornamentation)

add_executable(test_architecture_transform test_architecture_transform.c)
add_test(NAME architecture_transform COMMAND test_architecture_transform
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
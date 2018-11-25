add_executable(test_ursatz test_ursatz.c)
target_link_libraries(test_ursatz PRIVATE ursatz)
add_test(NAME ursatz_bootstrap COMMAND test_ursatz)

add_executable(test_entity_id test_entity_id.c)
target_link_libraries(test_entity_id PRIVATE ursatz)
add_test(NAME entity_id COMMAND test_entity_id)

add_executable(test_version test_version.c)
target_link_libraries(test_version PRIVATE ursatz)
add_test(NAME version COMMAND test_version)
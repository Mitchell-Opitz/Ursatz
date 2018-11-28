add_executable(test_entity_id test_entity_id.c)
target_link_libraries(test_entity_id PRIVATE ursatz)
add_test(NAME entity_id COMMAND test_entity_id)

add_executable(test_version test_version.c)
target_link_libraries(test_version PRIVATE ursatz)
add_test(NAME version COMMAND test_version)

add_executable(test_version_model test_version_model.c)
target_link_libraries(test_version_model PRIVATE ursatz)
add_test(NAME version_model COMMAND test_version_model)

add_executable(test_schema_version test_schema_version.c)
target_link_libraries(test_schema_version PRIVATE ursatz)
add_test(NAME schema_version COMMAND test_schema_version)

add_executable(test_architecture_identity test_architecture_identity.c)
add_test(NAME architecture_identity COMMAND test_architecture_identity
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
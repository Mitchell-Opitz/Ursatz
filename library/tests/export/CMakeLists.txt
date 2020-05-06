add_ursatz_test(musicxml_export)

add_executable(test_musicxml_export_mozart545 test_musicxml_export_mozart545.c)
target_link_libraries(test_musicxml_export_mozart545 PRIVATE ursatz)
add_test(NAME musicxml_export_mozart545 COMMAND test_musicxml_export_mozart545
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_musicxml_export_bach_prelude test_musicxml_export_bach_prelude.c)
target_link_libraries(test_musicxml_export_bach_prelude PRIVATE ursatz)
add_test(NAME musicxml_export_bach_prelude COMMAND test_musicxml_export_bach_prelude
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_ursatz_architecture_test(export)

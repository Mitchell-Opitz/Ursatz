add_executable(test_structure test_structure.c)
target_link_libraries(test_structure PRIVATE ursatz)
add_test(NAME structure COMMAND test_structure)

add_executable(test_motif test_motif.c)
target_link_libraries(test_motif PRIVATE ursatz)
add_test(NAME motif COMMAND test_motif)

add_executable(test_motif_occurrence test_motif_occurrence.c)
target_link_libraries(test_motif_occurrence PRIVATE ursatz)
add_test(NAME motif_occurrence COMMAND test_motif_occurrence)

add_executable(test_cadence test_cadence.c)
target_link_libraries(test_cadence PRIVATE ursatz)
add_test(NAME cadence COMMAND test_cadence)

add_executable(test_architecture_structure test_architecture_structure.c)
add_test(NAME architecture_structure COMMAND test_architecture_structure
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
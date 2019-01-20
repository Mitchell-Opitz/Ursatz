add_executable(test_structure test_structure.c)
target_link_libraries(test_structure PRIVATE ursatz)
add_test(NAME structure COMMAND test_structure)

add_executable(test_motif test_motif.c)
target_link_libraries(test_motif PRIVATE ursatz)
add_test(NAME motif COMMAND test_motif)

add_executable(test_architecture_structure test_architecture_structure.c)
add_test(NAME architecture_structure COMMAND test_architecture_structure
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
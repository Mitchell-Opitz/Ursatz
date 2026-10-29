add_executable(test_architecture_chain test_architecture_chain.c)
add_test(NAME architecture_chain COMMAND test_architecture_chain
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_chord_identification test_chord_identification.c)
target_link_libraries(test_chord_identification PRIVATE ursatz_chain)
add_test(NAME chord_identification COMMAND test_chord_identification)

add_executable(test_key_estimation test_key_estimation.c)
target_link_libraries(test_key_estimation PRIVATE ursatz_chain)
add_test(NAME key_estimation COMMAND test_key_estimation)

# The two fixture tests keep their ../fixtures paths by running from tests/analysis.
add_executable(test_bach_prelude_arpeggiated_chords test_bach_prelude_arpeggiated_chords.c)
target_link_libraries(test_bach_prelude_arpeggiated_chords PRIVATE ursatz_chain)
add_test(NAME bach_prelude_arpeggiated_chords COMMAND test_bach_prelude_arpeggiated_chords
         WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/analysis)

add_executable(test_framework_v1_corpus_subset_match_report test_framework_v1_corpus_subset_match_report.c)
target_link_libraries(test_framework_v1_corpus_subset_match_report PRIVATE ursatz_chain)
add_test(NAME framework_v1_corpus_subset_match_report COMMAND test_framework_v1_corpus_subset_match_report
         WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests/analysis)

add_executable(test_form_bridge test_form_bridge.c)
target_link_libraries(test_form_bridge PRIVATE ursatz_chain)
add_test(NAME chain_form_bridge COMMAND test_form_bridge)

add_executable(test_chain_inputs test_chain_inputs.c)
target_link_libraries(test_chain_inputs PRIVATE ursatz_chain)
add_test(NAME chain_inputs COMMAND test_chain_inputs)

# Memcheck for the bridges and the input builders. Skips silently if valgrind isn't installed.
find_program(URSATZ_CHAIN_VALGRIND valgrind)
if(URSATZ_CHAIN_VALGRIND)
  add_test(NAME chain_form_bridge_valgrind
           COMMAND ${URSATZ_CHAIN_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_form_bridge>)
  add_test(NAME chain_inputs_valgrind
           COMMAND ${URSATZ_CHAIN_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_chain_inputs>)
endif()

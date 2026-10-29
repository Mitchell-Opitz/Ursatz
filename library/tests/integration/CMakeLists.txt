# Integration tests chain several frameworks. Framework sources never do; these may.
add_executable(test_mozart545_phrase_model test_mozart545_phrase_model.c)
target_link_libraries(test_mozart545_phrase_model PRIVATE ursatz_chain standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_mozart545_phrase_model PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_mozart545_phrase_model PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME mozart545_phrase_model COMMAND test_mozart545_phrase_model)

add_executable(test_cadences_reference_pieces test_cadences_reference_pieces.c)
target_link_libraries(test_cadences_reference_pieces PRIVATE ursatz_chain standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_cadences_reference_pieces PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_cadences_reference_pieces PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME cadences_reference_pieces COMMAND test_cadences_reference_pieces)

add_executable(test_voice_leading_report test_voice_leading_report.c)
target_link_libraries(test_voice_leading_report PRIVATE ursatz_chain standard_counterpoint standard_harmony standard_meter)
target_include_directories(test_voice_leading_report PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_voice_leading_report PRIVATE
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_VOICE_LEADING_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/voice_leading")
add_test(NAME voice_leading_report COMMAND test_voice_leading_report)

add_executable(test_melody_bach114 test_melody_bach114.c)
target_link_libraries(test_melody_bach114 PRIVATE ursatz_chain standard_counterpoint common_practice_minimal standard_meter)
target_include_directories(test_melody_bach114 PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_melody_bach114 PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME melody_bach114 COMMAND test_melody_bach114)

add_executable(test_form_reference_pieces test_form_reference_pieces.c)
target_link_libraries(test_form_reference_pieces PRIVATE ursatz_chain standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_form_reference_pieces PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_form_reference_pieces PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME form_reference_pieces COMMAND test_form_reference_pieces)

add_executable(test_motive_mozart545 test_motive_mozart545.c)
target_link_libraries(test_motive_mozart545 PRIVATE ursatz_chain standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_motive_mozart545 PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_motive_mozart545 PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME motive_mozart545 COMMAND test_motive_mozart545)

add_executable(test_rhythm_field5 test_rhythm_field5.c)
target_link_libraries(test_rhythm_field5 PRIVATE ursatz_chain standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_rhythm_field5 PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_rhythm_field5 PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME rhythm_field5 COMMAND test_rhythm_field5)

add_executable(test_texture_field1 test_texture_field1.c)
target_link_libraries(test_texture_field1 PRIVATE ursatz_chain standard_texture standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_texture_field1 PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_texture_field1 PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME texture_field1 COMMAND test_texture_field1)

add_executable(test_schema_sequence test_schema_sequence.c)
target_link_libraries(test_schema_sequence PRIVATE ursatz_chain galant_schemas scale_harmonization standard_harmony standard_tonality
                      common_practice_minimal standard_meter)
target_include_directories(test_schema_sequence PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_schema_sequence PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/galant"
    URSATZ_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME schema_sequence COMMAND test_schema_sequence)

add_executable(test_framework_v1_verification framework_v1_verification.c)
target_link_libraries(test_framework_v1_verification PRIVATE ursatz_chain standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_framework_v1_verification PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_framework_v1_verification PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_DEVIATIONS_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus/expected_deviations.md"
    URSATZ_VERIFICATION_SUMMARY="${CMAKE_CURRENT_BINARY_DIR}/framework_v1_verification_summary.txt")
add_test(NAME framework_v1_verification COMMAND test_framework_v1_verification)

add_executable(test_framework_v1_thematic_returns framework_v1_thematic_returns.c)
target_link_libraries(test_framework_v1_thematic_returns PRIVATE ursatz_chain standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_framework_v1_thematic_returns PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_framework_v1_thematic_returns PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_DEVIATIONS_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus/expected_deviations.md"
    URSATZ_RETURNS_SUMMARY="${CMAKE_CURRENT_BINARY_DIR}/framework_v1_returns_summary.txt")
add_test(NAME framework_v1_thematic_returns COMMAND test_framework_v1_thematic_returns)

add_executable(test_corpus_style_statistics test_corpus_style_statistics.c)
target_link_libraries(test_corpus_style_statistics PRIVATE ursatz_chain standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_corpus_style_statistics PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis
                           ${PROJECT_SOURCE_DIR}/tests/corpus_stats)
target_compile_definitions(test_corpus_style_statistics PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_DEVIATIONS_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus/expected_deviations.md")
add_test(NAME corpus_style_statistics COMMAND test_corpus_style_statistics)

add_executable(test_corpus_similarity_search test_corpus_similarity_search.c)
target_link_libraries(test_corpus_similarity_search PRIVATE ursatz_chain standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_corpus_similarity_search PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis
                           ${PROJECT_SOURCE_DIR}/tests/corpus_stats)
target_compile_definitions(test_corpus_similarity_search PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_DEVIATIONS_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus/expected_deviations.md")
add_test(NAME corpus_similarity_search COMMAND test_corpus_similarity_search)

add_executable(test_persistence_full_analysis test_persistence_full_analysis.c)
target_link_libraries(test_persistence_full_analysis PRIVATE ursatz_chain standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_persistence_full_analysis PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_persistence_full_analysis PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_DEVIATIONS_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus/expected_deviations.md")
add_test(NAME persistence_full_analysis COMMAND test_persistence_full_analysis)

# Not run under valgrind: standard_form_find_motives (frameworks/standard-form/analysis/analysis_motive.c, make_kernel_window)
# leaks 66 KB on this piece, which is the analysis' own and not the persistence code's -- valgrind reports no frame in
# persistence/. The persistence repositories are leak-checked by their own tests in tests/persistence.

# Eight threads make the first call of every framework accessor and one rule accessor per framework together (core/once.h).
# Under helgrind where valgrind is installed (memcheck as well), else plain.
add_executable(test_concurrent_first_use test_concurrent_first_use.c)
target_link_libraries(test_concurrent_first_use PRIVATE baroque_ground_bass common_practice_minimal galant_schemas
                      high_baroque_fugue scale_harmonization sixteenth_century_style standard_counterpoint standard_form
                      standard_harmony standard_markings standard_meter standard_notation standard_rhythm
                      standard_texture standard_tonality standard_voicing western_periods)
add_test(NAME concurrent_first_use COMMAND test_concurrent_first_use)
find_program(URSATZ_INTEGRATION_VALGRIND valgrind)
if(URSATZ_INTEGRATION_VALGRIND)
  add_test(NAME concurrent_first_use_valgrind
           COMMAND ${URSATZ_INTEGRATION_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_concurrent_first_use>)
  add_test(NAME concurrent_first_use_helgrind
           COMMAND ${URSATZ_INTEGRATION_VALGRIND} --tool=helgrind --error-exitcode=1
                   $<TARGET_FILE:test_concurrent_first_use>)
endif()

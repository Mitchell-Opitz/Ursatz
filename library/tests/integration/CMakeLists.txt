# Integration tests chain several frameworks. Framework sources never do; these may.
add_executable(test_mozart545_phrase_model test_mozart545_phrase_model.c)
target_link_libraries(test_mozart545_phrase_model PRIVATE standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_mozart545_phrase_model PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_mozart545_phrase_model PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME mozart545_phrase_model COMMAND test_mozart545_phrase_model)

add_executable(test_cadences_reference_pieces test_cadences_reference_pieces.c)
target_link_libraries(test_cadences_reference_pieces PRIVATE standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_cadences_reference_pieces PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_cadences_reference_pieces PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME cadences_reference_pieces COMMAND test_cadences_reference_pieces)

add_executable(test_voice_leading_report test_voice_leading_report.c)
target_link_libraries(test_voice_leading_report PRIVATE standard_counterpoint standard_harmony standard_meter)
target_include_directories(test_voice_leading_report PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_voice_leading_report PRIVATE
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_VOICE_LEADING_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/voice_leading")
add_test(NAME voice_leading_report COMMAND test_voice_leading_report)

add_executable(test_melody_bach114 test_melody_bach114.c)
target_link_libraries(test_melody_bach114 PRIVATE standard_counterpoint common_practice_minimal standard_meter)
target_include_directories(test_melody_bach114 PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_melody_bach114 PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME melody_bach114 COMMAND test_melody_bach114)

add_executable(test_form_reference_pieces test_form_reference_pieces.c)
target_link_libraries(test_form_reference_pieces PRIVATE standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_form_reference_pieces PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_form_reference_pieces PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME form_reference_pieces COMMAND test_form_reference_pieces)

add_executable(test_motive_mozart545 test_motive_mozart545.c)
target_link_libraries(test_motive_mozart545 PRIVATE standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_motive_mozart545 PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_motive_mozart545 PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME motive_mozart545 COMMAND test_motive_mozart545)

add_executable(test_rhythm_field5 test_rhythm_field5.c)
target_link_libraries(test_rhythm_field5 PRIVATE standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_rhythm_field5 PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_rhythm_field5 PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME rhythm_field5 COMMAND test_rhythm_field5)

add_executable(test_texture_field1 test_texture_field1.c)
target_link_libraries(test_texture_field1 PRIVATE standard_texture standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_texture_field1 PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_texture_field1 PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME texture_field1 COMMAND test_texture_field1)

add_executable(test_schema_sequence test_schema_sequence.c)
target_link_libraries(test_schema_sequence PRIVATE galant_schemas scale_harmonization standard_harmony standard_tonality
                      common_practice_minimal standard_meter)
target_include_directories(test_schema_sequence PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_schema_sequence PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/galant"
    URSATZ_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME schema_sequence COMMAND test_schema_sequence)

add_executable(test_framework_v1_verification framework_v1_verification.c)
target_link_libraries(test_framework_v1_verification PRIVATE standard_form standard_harmony standard_tonality common_practice_minimal
                      standard_meter)
target_include_directories(test_framework_v1_verification PRIVATE ${PROJECT_SOURCE_DIR}/tests/analysis)
target_compile_definitions(test_framework_v1_verification PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_DEVIATIONS_FILE="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus/expected_deviations.md"
    URSATZ_VERIFICATION_SUMMARY="${CMAKE_CURRENT_BINARY_DIR}/framework_v1_verification_summary.txt")
add_test(NAME framework_v1_verification COMMAND test_framework_v1_verification)

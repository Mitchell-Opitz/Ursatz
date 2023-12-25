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

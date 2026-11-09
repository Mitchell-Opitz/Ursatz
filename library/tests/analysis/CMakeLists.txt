add_ursatz_test(analyzer)
add_ursatz_test(feature_extraction)
add_ursatz_test(interval_analysis)
add_ursatz_test(scale_analysis)

add_ursatz_test(voice_leading_analysis)
add_ursatz_test(outer_voice)
add_ursatz_test(outer_voice_attack_soprano)
add_ursatz_test(voice_separation)
add_ursatz_test(cadence_detection)
add_ursatz_test(cadence_classification)
add_ursatz_test(motif_window_matching)
add_ursatz_test(motive_sketch)
add_ursatz_test(motive_sketch_value)
add_ursatz_test(motive_relation)
add_ursatz_test(motive_relation_params)
add_ursatz_test(motive_tone_roles)
add_ursatz_test(motive_variant_recognition)
# 130 s measured (serial, 10 s rule); runs in the nightly.
set_tests_properties(motive_variant_recognition PROPERTIES LABELS slow)
add_ursatz_test(motive_variant_recognition_chains)
# Every frame relation in a chain of two with every other case, both orders (about 2 minutes); CI runs ctest -LE slow.
add_test(NAME motive_variant_recognition_frame_chains COMMAND test_motive_variant_recognition frame-chains)
set_tests_properties(motive_variant_recognition_frame_chains PROPERTIES LABELS slow)
add_ursatz_test(motive_link)
add_ursatz_test(motive_link_groups)
add_ursatz_test(progression_patterns)

# Scoped memcheck for the motive link test only (not a repo-wide valgrind mechanism -- see
# tests/persistence/CMakeLists.txt for the same pattern). Skips silently if valgrind isn't installed.
find_program(URSATZ_ANALYSIS_VALGRIND valgrind)
if(URSATZ_ANALYSIS_VALGRIND)
  add_test(NAME motive_link_valgrind
           COMMAND ${URSATZ_ANALYSIS_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_motive_link>)
  # 18 s measured (serial, 10 s rule); runs in the nightly.
  set_tests_properties(motive_link_valgrind PROPERTIES LABELS slow)
endif()

# Scoped memcheck for the motive tone roles test, same pattern.
if(URSATZ_ANALYSIS_VALGRIND)
  add_test(NAME motive_tone_roles_valgrind
           COMMAND ${URSATZ_ANALYSIS_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_motive_tone_roles>)
endif()

# Scoped memcheck for the motive sketch value test, same pattern.
if(URSATZ_ANALYSIS_VALGRIND)
  add_test(NAME motive_sketch_value_valgrind
           COMMAND ${URSATZ_ANALYSIS_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_motive_sketch_value>)
endif()

# Scoped memcheck for the motive relation parameter test, same pattern.
if(URSATZ_ANALYSIS_VALGRIND)
  add_test(NAME motive_relation_params_valgrind
           COMMAND ${URSATZ_ANALYSIS_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_motive_relation_params>)
endif()

# Scoped memcheck for the attack soprano test, same pattern.
if(URSATZ_ANALYSIS_VALGRIND)
  add_test(NAME outer_voice_attack_soprano_valgrind
           COMMAND ${URSATZ_ANALYSIS_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_outer_voice_attack_soprano>)
endif()

# Scoped memcheck for the progression patterns and motive link groups tests, same pattern.
if(URSATZ_ANALYSIS_VALGRIND)
  add_test(NAME progression_patterns_valgrind
           COMMAND ${URSATZ_ANALYSIS_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_progression_patterns>)
  add_test(NAME motive_link_groups_valgrind
           COMMAND ${URSATZ_ANALYSIS_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_motive_link_groups>)
endif()
add_ursatz_test(phrase_segmentation)
add_ursatz_test(phrase_boundary_detection)
add_ursatz_test(period_detection)
add_ursatz_test(sentence_continuation_matching)
add_ursatz_test(sentence_detection)
add_ursatz_test(form_structure_detection)
add_ursatz_test(thematic_return_detection)
add_executable(test_field1_weighted_harmony test_field1_weighted_harmony.c)
target_link_libraries(test_field1_weighted_harmony PRIVATE ursatz_chain common_practice_minimal standard_meter)
target_compile_definitions(test_field1_weighted_harmony PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME field1_weighted_harmony COMMAND test_field1_weighted_harmony)

add_executable(test_reference_harmony test_reference_harmony.c)
target_link_libraries(test_reference_harmony PRIVATE ursatz_chain common_practice_minimal standard_meter)
target_compile_definitions(test_reference_harmony PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME reference_harmony COMMAND test_reference_harmony)

add_executable(test_reference_phrases test_reference_phrases.c)
target_link_libraries(test_reference_phrases PRIVATE ursatz_chain common_practice_minimal standard_meter)
target_compile_definitions(test_reference_phrases PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_test(NAME reference_phrases COMMAND test_reference_phrases)
if(URSATZ_ANALYSIS_VALGRIND)
  add_test(NAME reference_phrases_valgrind
           COMMAND ${URSATZ_ANALYSIS_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_reference_phrases>)
  # 174 s measured (serial); runs in the nightly.
  set_tests_properties(reference_phrases_valgrind PROPERTIES LABELS slow)
endif()

add_executable(test_reference_repeats test_reference_repeats.c)
target_link_libraries(test_reference_repeats PRIVATE ursatz_chain common_practice_minimal standard_meter)
target_compile_definitions(test_reference_repeats PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_test(NAME reference_repeats COMMAND test_reference_repeats)
if(URSATZ_ANALYSIS_VALGRIND)
  add_test(NAME reference_repeats_valgrind
           COMMAND ${URSATZ_ANALYSIS_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_reference_repeats>)
  set_tests_properties(reference_repeats_valgrind PROPERTIES LABELS slow)
endif()

add_executable(test_two_voice_spans test_two_voice_spans.c)
target_link_libraries(test_two_voice_spans PRIVATE ursatz_chain common_practice_minimal standard_meter)
target_compile_definitions(test_two_voice_spans PRIVATE
    URSATZ_TWO_VOICE_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/two-voice")
add_test(NAME two_voice_spans COMMAND test_two_voice_spans)

add_executable(test_weighted_coverage_report test_weighted_coverage_report.c)
target_link_libraries(test_weighted_coverage_report PRIVATE ursatz_chain common_practice_minimal standard_meter)
target_compile_definitions(test_weighted_coverage_report PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_test(NAME weighted_coverage_report COMMAND test_weighted_coverage_report)

add_executable(test_outer_voices_written_parts test_outer_voices_written_parts.c)
target_link_libraries(test_outer_voices_written_parts PRIVATE ursatz_chain common_practice_minimal standard_meter standard_tonality)
target_compile_definitions(test_outer_voices_written_parts PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME outer_voices_written_parts COMMAND test_outer_voices_written_parts)

add_executable(test_voice_separation_corpus test_voice_separation_corpus.c)
target_link_libraries(test_voice_separation_corpus PRIVATE standard_meter standard_tonality)
target_compile_definitions(test_voice_separation_corpus PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME voice_separation_corpus COMMAND test_voice_separation_corpus)

add_executable(test_key_regions_corpus test_key_regions_corpus.c)
target_link_libraries(test_key_regions_corpus PRIVATE ursatz_chain common_practice_minimal standard_meter)
target_compile_definitions(test_key_regions_corpus PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME key_regions_corpus COMMAND test_key_regions_corpus)

add_ursatz_architecture_test(analysis)

add_executable(test_bach114_roman_numerals test_bach114_roman_numerals.c)
target_link_libraries(test_bach114_roman_numerals PRIVATE ursatz_chain standard_tonality common_practice_minimal standard_meter)
target_compile_definitions(test_bach114_roman_numerals PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME bach114_roman_numerals COMMAND test_bach114_roman_numerals)

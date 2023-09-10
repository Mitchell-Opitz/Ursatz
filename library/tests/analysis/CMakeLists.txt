add_ursatz_test(analyzer)
add_ursatz_test(feature_extraction)
add_ursatz_test(interval_analysis)
add_ursatz_test(scale_analysis)

# chord_identification.c and key_estimation.c are compiled into
# common_practice_minimal (frameworks/common-practice-minimal/CMakeLists.txt),
# not ursatz -- see the comment in src/CMakeLists.txt. Their tests link
# that library instead of the default add_ursatz_test(ursatz) link.
add_executable(test_chord_identification test_chord_identification.c)
target_link_libraries(test_chord_identification PRIVATE common_practice_minimal)
add_test(NAME chord_identification COMMAND test_chord_identification)

add_executable(test_key_estimation test_key_estimation.c)
target_link_libraries(test_key_estimation PRIVATE common_practice_minimal)
add_test(NAME key_estimation COMMAND test_key_estimation)

add_ursatz_test(voice_leading_analysis)
add_ursatz_test(outer_voice)
add_ursatz_test(voice_separation)
add_ursatz_test(cadence_detection)
add_ursatz_test(cadence_classification)
add_ursatz_test(motif_detection)
add_ursatz_test(motif_window_matching)
add_ursatz_test(phrase_segmentation)
add_ursatz_test(phrase_boundary_detection)
add_ursatz_test(period_detection)
add_ursatz_test(sentence_continuation_matching)
add_ursatz_test(sentence_detection)
add_ursatz_test(form_structure_detection)
add_ursatz_test(thematic_return_detection)
add_executable(test_bach_prelude_arpeggiated_chords test_bach_prelude_arpeggiated_chords.c)
target_link_libraries(test_bach_prelude_arpeggiated_chords PRIVATE common_practice_minimal)
add_test(NAME bach_prelude_arpeggiated_chords COMMAND test_bach_prelude_arpeggiated_chords
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_framework_v1_corpus_subset_match_report test_framework_v1_corpus_subset_match_report.c)
target_link_libraries(test_framework_v1_corpus_subset_match_report PRIVATE common_practice_minimal)
add_test(NAME framework_v1_corpus_subset_match_report COMMAND test_framework_v1_corpus_subset_match_report
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_field1_weighted_harmony test_field1_weighted_harmony.c)
target_link_libraries(test_field1_weighted_harmony PRIVATE common_practice_minimal standard_meter)
target_compile_definitions(test_field1_weighted_harmony PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME field1_weighted_harmony COMMAND test_field1_weighted_harmony)

add_executable(test_weighted_coverage_report test_weighted_coverage_report.c)
target_link_libraries(test_weighted_coverage_report PRIVATE common_practice_minimal standard_meter)
target_compile_definitions(test_weighted_coverage_report PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_test(NAME weighted_coverage_report COMMAND test_weighted_coverage_report)

add_executable(test_outer_voices_written_parts test_outer_voices_written_parts.c)
target_link_libraries(test_outer_voices_written_parts PRIVATE common_practice_minimal standard_meter)
target_compile_definitions(test_outer_voices_written_parts PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME outer_voices_written_parts COMMAND test_outer_voices_written_parts)

add_executable(test_voice_separation_corpus test_voice_separation_corpus.c)
target_link_libraries(test_voice_separation_corpus PRIVATE standard_meter)
target_compile_definitions(test_voice_separation_corpus PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME voice_separation_corpus COMMAND test_voice_separation_corpus)

add_executable(test_key_regions_corpus test_key_regions_corpus.c)
target_link_libraries(test_key_regions_corpus PRIVATE common_practice_minimal standard_meter)
target_compile_definitions(test_key_regions_corpus PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")
add_test(NAME key_regions_corpus COMMAND test_key_regions_corpus)

add_ursatz_architecture_test(analysis)

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
add_ursatz_test(cadence_detection)
add_ursatz_test(cadence_classification)
add_ursatz_test(motif_detection)
add_ursatz_test(motif_window_matching)
add_ursatz_test(phrase_segmentation)
add_ursatz_test(phrase_boundary_detection)
add_ursatz_architecture_test(analysis)

ursatz_test(outer_voice LIBS ursatz)
ursatz_test(outer_voice_attack_soprano LIBS ursatz VALGRIND)
ursatz_test(voice_separation LIBS ursatz)
ursatz_test(motif_window_matching LIBS ursatz)
ursatz_test(motive_sketch LIBS ursatz)
ursatz_test(motive_sketch_value LIBS ursatz VALGRIND)
ursatz_test(motive_relation LIBS ursatz)
ursatz_test(motive_relation_params LIBS ursatz VALGRIND)
ursatz_test(motive_tone_roles LIBS ursatz VALGRIND)
# 130 s measured (serial, 10 s rule); runs in the nightly.
ursatz_test_executable(test_motive_variant_recognition LIBS ursatz)
ursatz_test(motive_variant_recognition TARGET test_motive_variant_recognition LABELS slow)
ursatz_test(motive_variant_recognition_chains LIBS ursatz)
# Every frame relation in a chain of two with every other case, both orders (about 2 minutes); CI runs ctest -LE slow.
ursatz_test(motive_variant_recognition_frame_chains TARGET test_motive_variant_recognition ARGS frame-chains
    LABELS slow)
ursatz_test(motive_link LIBS ursatz VALGRIND)
ursatz_test(motive_link_groups LIBS ursatz VALGRIND)
ursatz_test(progression_patterns LIBS ursatz VALGRIND)

ursatz_test(phrase_boundary_detection LIBS ursatz)
ursatz_test(form_structure_detection LIBS ursatz)
ursatz_test(thematic_return_detection LIBS ursatz)
ursatz_test(field1_weighted_harmony LIBS ursatz_chain common_practice_minimal standard_meter
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")

ursatz_test(reference_harmony LIBS ursatz_chain common_practice_minimal standard_meter
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")

ursatz_test(reference_phrases LIBS ursatz_chain common_practice_minimal standard_meter ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus" VALGRIND)

ursatz_test(reference_keys LIBS ursatz_chain common_practice_minimal standard_meter ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations" VALGRIND)

ursatz_test(reference_repeats LIBS ursatz_chain common_practice_minimal standard_meter ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus" VALGRIND)

ursatz_test(two_voice_spans LIBS ursatz_chain common_practice_minimal standard_meter
    DEFINITIONS URSATZ_TWO_VOICE_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/two-voice")

ursatz_test(outer_voices_written_parts LIBS ursatz_chain common_practice_minimal standard_meter standard_tonality
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")

ursatz_test(voice_separation_corpus LIBS standard_meter standard_tonality ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")

ursatz_test(key_regions_corpus LIBS ursatz_chain common_practice_minimal standard_meter
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")

ursatz_test(bach114_roman_numerals LIBS ursatz_chain standard_tonality common_practice_minimal standard_meter
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations")

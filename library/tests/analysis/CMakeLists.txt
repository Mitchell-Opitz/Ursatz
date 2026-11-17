ursatz_test(field1_weighted_harmony LIBS ursatz_chain ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(reference_harmony LIBS ursatz_chain ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(reference_phrases LIBS ursatz_chain ursatz_analysis ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)

ursatz_test(reference_keys LIBS ursatz_chain ursatz_analysis ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations" VALGRIND)

ursatz_test(reference_repeats LIBS ursatz_chain ursatz_analysis ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)

ursatz_test(two_voice_spans LIBS ursatz_chain ursatz_analysis
    DEFINITIONS URSATZ_TWO_VOICE_DIR="${URSATZ_FIXTURES_DIR}/two-voice")

ursatz_test(outer_voices_written_parts LIBS ursatz_chain ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(key_regions_corpus LIBS ursatz_chain ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(bach114_roman_numerals LIBS ursatz_chain ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

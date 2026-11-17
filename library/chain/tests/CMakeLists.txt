ursatz_test_includes()

ursatz_test_executable(test_form_bridge LIBS ursatz_chain)
ursatz_test(chain_form_bridge TARGET test_form_bridge VALGRIND)

ursatz_test(chain_inputs LIBS ursatz_chain ursatz_io VALGRIND)

# The chain on four reference pieces against fixed expected results (captured while it was proven equal to the test glue it replaced).
# 13 s measured (serial, 10 s rule); runs in the nightly.
ursatz_test(analysis_chain LIBS ursatz_chain ursatz_io ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" LABELS slow)

# The smoke: Field 1 only, the smallest fixture (0.3 s), in the fast set.
ursatz_test(analysis_chain_field1 TARGET test_analysis_chain ENVIRONMENT URSATZ_CHAIN_PIECE=field1)

# The smallest fixture only, to keep the memcheck run short.
if(URSATZ_VALGRIND)
    ursatz_test(analysis_chain_valgrind
        COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS} $<TARGET_FILE:test_analysis_chain>
        ENVIRONMENT "URSATZ_CHAIN_PIECE=field1" LABELS slow valgrind)
endif()

ursatz_test_executable(test_kernel_owners LIBS ursatz_chain)
ursatz_test(chain_kernel_owners TARGET test_kernel_owners VALGRIND)

ursatz_test(field1_weighted_harmony LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(reference_harmony LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(reference_phrases LIBS ursatz_chain ursatz_io ursatz_analysis ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)

ursatz_test(reference_keys LIBS ursatz_chain ursatz_io ursatz_analysis ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations" VALGRIND)

ursatz_test(reference_repeats LIBS ursatz_chain ursatz_io ursatz_analysis ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)

ursatz_test(two_voice_spans LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_TWO_VOICE_DIR="${URSATZ_FIXTURES_DIR}/two-voice")

ursatz_test(outer_voices_written_parts LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(key_regions_corpus LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(bach114_roman_numerals LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

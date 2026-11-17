# Integration tests chain several frameworks. Framework sources never do; these may.
ursatz_test(mozart545_phrase_model LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(cadences_reference_pieces LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(voice_leading_report LIBS ursatz_chain ursatz_analysis
    DEFINITIONS URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations"
    URSATZ_VOICE_LEADING_DIR="${URSATZ_FIXTURES_DIR}/voice_leading")

ursatz_test(melody_bach114 LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(form_reference_pieces LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(motive_mozart545 LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations" VALGRIND)

ursatz_test(rhythm_field5 LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(texture_field1 LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(schema_sequence LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations"
    URSATZ_FIXTURE_DIR="${URSATZ_FIXTURES_DIR}/galant"
    URSATZ_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")

ursatz_test(framework_v1_verification SOURCES framework_v1_verification.c LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations"
    URSATZ_DEVIATIONS_FILE="${URSATZ_FIXTURES_DIR}/framework-v1-corpus/expected_deviations.md"
    URSATZ_VERIFICATION_SUMMARY="${CMAKE_CURRENT_BINARY_DIR}/framework_v1_verification_summary.txt")

ursatz_test(framework_v1_thematic_returns SOURCES framework_v1_thematic_returns.c LIBS ursatz_chain ursatz_io ursatz_analysis
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations"
    URSATZ_DEVIATIONS_FILE="${URSATZ_FIXTURES_DIR}/framework-v1-corpus/expected_deviations.md"
    URSATZ_RETURNS_SUMMARY="${CMAKE_CURRENT_BINARY_DIR}/framework_v1_returns_summary.txt")

# Eight threads make the first call of every framework accessor and every preference accessor an app calls together (core/once.h).
# Under helgrind where valgrind is installed (memcheck as well), else plain.
ursatz_test(concurrent_first_use LIBS ursatz_analysis VALGRIND)
# Helgrind over the same executable, nightly like the memcheck variants.
if(URSATZ_VALGRIND)
    ursatz_test(concurrent_first_use_helgrind
        COMMAND ${URSATZ_VALGRIND} --tool=helgrind --error-exitcode=1 $<TARGET_FILE:test_concurrent_first_use>
        LABELS slow valgrind)
endif()

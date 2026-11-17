ursatz_test(outer_voice LIBS ursatz_analysis)

ursatz_test(outer_voice_attack_soprano LIBS ursatz_analysis VALGRIND)

ursatz_test(voice_separation LIBS ursatz_analysis)

ursatz_test(voice_separation_corpus LIBS ursatz_analysis ursatz ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

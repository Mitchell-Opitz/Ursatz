ursatz_test(musicxml_score LIBS ursatz VALGRIND)
ursatz_test(musicxml_score_values LIBS ursatz VALGRIND)
ursatz_test(musicxml_score_beams LIBS ursatz VALGRIND)
ursatz_test(musicxml_score_reference LIBS ursatz
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus" VALGRIND)
ursatz_test(musicxml_score_overlaps LIBS ursatz ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_MUSICXML_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/musicxml" VALGRIND)
ursatz_test(musicxml_score_errors LIBS ursatz VALGRIND)
ursatz_test(musicxml_score_fixtures LIBS ursatz
    DEFINITIONS URSATZ_MUSICXML_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/musicxml" VALGRIND)
ursatz_test(score_quantize LIBS ursatz)
ursatz_test(score_staves LIBS ursatz)
ursatz_test(score_clefs LIBS ursatz VALGRIND)

ursatz_test(musicxml_export_mozart545 LIBS ursatz ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

ursatz_test(musicxml_export_bach_prelude LIBS ursatz ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

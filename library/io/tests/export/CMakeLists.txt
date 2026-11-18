ursatz_test(musicxml_score LIBS ursatz_io VALGRIND)
ursatz_test(musicxml_score_values LIBS ursatz_io VALGRIND)
ursatz_test(musicxml_score_beams LIBS ursatz_io VALGRIND)
ursatz_test(musicxml_score_reference LIBS ursatz_io
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(musicxml_score_overlaps LIBS ursatz_io ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_MUSICXML_DIR="${URSATZ_FIXTURES_DIR}/musicxml" VALGRIND)
ursatz_test(musicxml_score_errors LIBS ursatz_io VALGRIND)
ursatz_test(musicxml_score_fixtures LIBS ursatz_io
    DEFINITIONS URSATZ_MUSICXML_DIR="${URSATZ_FIXTURES_DIR}/musicxml" VALGRIND)
ursatz_test(musicxml_tuplets LIBS ursatz_io
    DEFINITIONS URSATZ_MUSICXML_DIR="${URSATZ_FIXTURES_DIR}/musicxml"
    URSATZ_ANALYZER_MUSICXML_DIR="${CMAKE_SOURCE_DIR}/analyzer/tests/musicxml")
ursatz_test(score_quantize LIBS ursatz_io)
ursatz_test(score_staves LIBS ursatz_io)
ursatz_test(score_clefs LIBS ursatz_io VALGRIND)

ursatz_test(musicxml_export_mozart545 LIBS ursatz_io ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

ursatz_test(musicxml_export_bach_prelude LIBS ursatz_io ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
ursatz_test(musicxml_pedals LIBS ursatz_io VALGRIND)
ursatz_test(movement_export LIBS ursatz_io VALGRIND)

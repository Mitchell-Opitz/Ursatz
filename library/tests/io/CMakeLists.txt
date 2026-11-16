ursatz_test(smf_reader LIBS ursatz VALGRIND)
ursatz_test(midi_event_decoder LIBS ursatz VALGRIND)
ursatz_test(midi_importer LIBS ursatz VALGRIND)
ursatz_test(midi_renderer LIBS ursatz VALGRIND)
ursatz_test(midi_renderer_notes LIBS ursatz VALGRIND)
ursatz_test(midi_renderer_errors LIBS ursatz VALGRIND)
ursatz_test(midi_renderer_meta LIBS ursatz VALGRIND)
ursatz_test(midi_renderer_roundtrip LIBS ursatz ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(midi_key_signature LIBS ursatz
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(midi_meter LIBS ursatz VALGRIND)
ursatz_test(midi_tempo LIBS ursatz VALGRIND)
ursatz_test(midi_import_complete LIBS ursatz
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(midi_corpus_meter LIBS ursatz
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(midi_corpus_tempo LIBS ursatz
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(musicxml_importer LIBS ursatz
    DEFINITIONS URSATZ_MUSICXML_DIR="${URSATZ_FIXTURES_DIR}/musicxml" VALGRIND)
ursatz_test(musicxml_roundtrip LIBS ursatz ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus")

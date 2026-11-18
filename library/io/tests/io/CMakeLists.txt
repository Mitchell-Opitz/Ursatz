ursatz_test(smf_reader LIBS ursatz_io VALGRIND)
ursatz_test(midi_event_decoder LIBS ursatz_io VALGRIND)
ursatz_test(midi_importer LIBS ursatz_io VALGRIND)
ursatz_test(midi_renderer LIBS ursatz_io VALGRIND)
ursatz_test(midi_renderer_notes LIBS ursatz_io VALGRIND)
ursatz_test(midi_renderer_errors LIBS ursatz_io VALGRIND)
ursatz_test(midi_renderer_meta LIBS ursatz_io VALGRIND)
ursatz_test(midi_renderer_roundtrip LIBS ursatz_io ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(midi_key_signature LIBS ursatz_io
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(midi_meter LIBS ursatz_io VALGRIND)
ursatz_test(midi_tempo LIBS ursatz_io VALGRIND)
ursatz_test(midi_import_complete LIBS ursatz_io
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(midi_corpus_meter LIBS ursatz_io
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(midi_corpus_tempo LIBS ursatz_io
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" VALGRIND)
ursatz_test(musicxml_importer LIBS ursatz_io
    DEFINITIONS URSATZ_MUSICXML_DIR="${URSATZ_FIXTURES_DIR}/musicxml" VALGRIND)
ursatz_test(musicxml_roundtrip LIBS ursatz_io ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus")
ursatz_test(midi_renderer_parts LIBS ursatz_io VALGRIND)

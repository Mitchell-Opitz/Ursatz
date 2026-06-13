add_ursatz_test(smf_reader)
add_ursatz_test(midi_event_decoder)
add_ursatz_test(midi_importer)
add_ursatz_test(midi_renderer)
add_ursatz_test(midi_renderer_notes)
add_ursatz_test(midi_renderer_errors)
add_ursatz_test(midi_renderer_meta)
add_ursatz_test(midi_renderer_roundtrip)
target_compile_definitions(test_midi_renderer_roundtrip PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_ursatz_test(midi_key_signature)
add_ursatz_test(midi_meter)
add_ursatz_test(midi_tempo)
add_ursatz_test(midi_corpus_meter)
target_compile_definitions(test_midi_corpus_meter PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_ursatz_test(midi_corpus_tempo)
target_compile_definitions(test_midi_corpus_tempo PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
target_compile_definitions(test_midi_key_signature PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_ursatz_test(musicxml_importer)
target_compile_definitions(test_musicxml_importer PRIVATE
    URSATZ_MUSICXML_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/musicxml")
add_ursatz_test(musicxml_roundtrip)
target_compile_definitions(test_musicxml_roundtrip PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_ursatz_architecture_test(io)

# Scoped memcheck coverage for this module's own test binaries only
# (not a repo-wide valgrind mechanism -- see docs/modules/io.md). Skips
# silently if valgrind isn't installed.
find_program(URSATZ_IO_VALGRIND valgrind)
if(URSATZ_IO_VALGRIND)
  foreach(io_test_name smf_reader midi_event_decoder midi_importer midi_key_signature midi_meter midi_tempo midi_corpus_meter midi_corpus_tempo musicxml_importer midi_renderer midi_renderer_notes midi_renderer_errors midi_renderer_meta midi_renderer_roundtrip)
    add_test(NAME ${io_test_name}_valgrind
             COMMAND ${URSATZ_IO_VALGRIND} --leak-check=full --error-exitcode=1
                     $<TARGET_FILE:test_${io_test_name}>)
  endforeach()
endif()

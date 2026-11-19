# The io group (spec/target-architecture.md 7.2): MIDI and MusicXML import and export, reading and writing the model.
# Sources include headers by their path from library/ (io/<file>.h).
add_library(ursatz_io
    general_midi.c
    imported_realization.c
    imported_score.c
    midi_importer.c
    midi_event_decoder.c
    midi_key_signature.c
    smf_reader.c
    midi_renderer.c
    movement_export.c
    musicxml_importer.c
    musicxml_score.c
    score_clefs.c
    score_quantize.c
    score_staves.c
)
ursatz_public_headers(ursatz_io
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
    HEADERS
        io/imported_score.h
        io/imported_realization.h
        io/midi_importer.h
        io/midi_key_signature.h
        io/midi_renderer.h
        io/movement_export.h
        io/musicxml_export.h
        io/musicxml_importer.h
        io/score_layout.h
        io/score_quantize.h
)
# The XML parser used by io/musicxml_importer.c only (third_party/ezxml); PRIVATE so no consumer sees it.
target_link_libraries(ursatz_io PUBLIC ursatz_model ursatz_realization PRIVATE ursatz_ezxml)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

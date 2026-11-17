add_library(ursatz
    io/midi_importer.c
    io/midi_event_decoder.c
    io/midi_key_signature.c
    io/smf_reader.c
    io/midi_renderer.c
    io/musicxml_importer.c
    export/musicxml_score.c
    export/score_clefs.c
    export/score_quantize.c
    export/score_staves.c
)
# Public headers (R-223): the only kernel headers a consumer sees. Staged at configure time under
# ${PROJECT_BINARY_DIR}/public/ursatz; the source directory is private to the target.
ursatz_public_headers(ursatz
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    HEADERS
        export/musicxml_export.h
        export/score_quantize.h
        export/score_staves.h
        io/midi_importer.h
        io/midi_key_signature.h
        io/midi_renderer.h
        io/musicxml_importer.h
)

# The XML parser used by io/musicxml_importer.c only (third_party/ezxml); PRIVATE so no consumer sees it.
target_link_libraries(ursatz PRIVATE ursatz_ezxml)
# Import and export read only model types.
target_link_libraries(ursatz PUBLIC ursatz_model)
# Sources include model headers by their path from library/.
target_include_directories(ursatz PRIVATE ${PROJECT_SOURCE_DIR})

add_library(ursatz
    semantics/interval.c
    semantics/sonority.c
    semantics/sonority_segmentation.c
    semantics/rhythmic_pattern.c
    semantics/contour.c
    semantics/voice_part.c
    semantics/beat_strength.c
    semantics/non_chord_tone.c
    semantics/tone_line.c
    semantics/cadence_type.c
    semantics/motive_occurrence_kind.c
    semantics/applied_chord_kind.c
    semantics/key_change_extent.c
    semantics/analysis_note.c
    semantics/analysis_key.c
    theory/framework.c
    theory/framework_singleton.c
    rules/rule.c
    rules/constraint.c
    rules/preference.c
    theory/style_model.c
    theory/style_fraction.c
    theory/style_features.c
    theory/style_model_build.c
    theory/style_model_blend.c
    analysis/beat_grid_length.c
    analysis/outer_voice.c
    analysis/voice_separation.c
    analysis/motif_window_matching.c
    analysis/motive_sketch.c
    analysis/motive_relation.c
    analysis/motive_tone_roles.c
    analysis/motive_variant_recognition.c
    analysis/motive_link.c
    analysis/progression_patterns.c
    analysis/phrase_boundary_detection.c
    analysis/form_structure_detection.c
    analysis/thematic_return_detection.c
    io/midi_importer.c
    io/midi_event_decoder.c
    io/midi_key_signature.c
    io/smf_reader.c
    io/midi_renderer.c
    io/musicxml_importer.c
    query/query_fraction.c
    query/similarity_model.c
    query/similarity_scoring.c
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
        analysis/form_structure_detection.h
        analysis/motif_window_matching.h
        analysis/motive_link.h
        analysis/motive_relation.h
        analysis/motive_sketch.h
        analysis/motive_tone_roles.h
        analysis/motive_variant_recognition.h
        analysis/outer_voice.h
        analysis/phrase_boundary_detection.h
        analysis/progression_patterns.h
        analysis/thematic_return_detection.h
        analysis/voice_separation.h
        context/context.h
        export/musicxml_export.h
        export/score_quantize.h
        export/score_staves.h
        io/midi_importer.h
        io/midi_key_signature.h
        io/midi_renderer.h
        io/musicxml_importer.h
        query/similarity_model.h
        query/similarity_scoring.h
        rules/constraint.h
        rules/preference.h
        rules/rule.h
        semantics/analysis_key.h
        semantics/analysis_note.h
        semantics/analysis_segment.h
        semantics/applied_chord_kind.h
        semantics/beat_strength.h
        semantics/cadence_type.h
        semantics/contour.h
        semantics/interval.h
        semantics/key_change_extent.h
        semantics/motive_occurrence_kind.h
        semantics/motif_window.h
        semantics/non_chord_tone.h
        semantics/rhythmic_pattern.h
        semantics/sonority.h
        semantics/sonority_segmentation.h
        semantics/subphrase_role.h
        semantics/tone_line.h
        semantics/voice_grid.h
        semantics/voice_part.h
        theory/framework.h
        theory/framework_singleton.h
        theory/style_features.h
        theory/style_model.h
        theory/style_model_blend.h
        theory/style_model_build.h
)

# The XML parser used by io/musicxml_importer.c only (third_party/ezxml); PRIVATE so no consumer sees it.
target_link_libraries(ursatz PRIVATE ursatz_ezxml)
# The model group (lround, threads and the value types come through it).
target_link_libraries(ursatz PUBLIC ursatz_model)
# Sources include model headers by their path from library/.
target_include_directories(ursatz PRIVATE ${PROJECT_SOURCE_DIR})

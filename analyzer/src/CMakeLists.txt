add_library(ursatz_analyzer STATIC
    version.c
    pipeline/imported_notes.c
    pipeline/notation_reading.c
    pipeline/midi_pipeline.c
    pipeline/gm_names.c
    pipeline/musicxml_pipeline.c
    pipeline/input_pipeline.c
    pipeline/segmentation.c
    pipeline/pitch_class_util.c
    pipeline/spelling_util.c
    pipeline/key_region_pipeline.c
    pipeline/chord_pipeline.c
    pipeline/outer_voice_pipeline.c
    pipeline/roman_numeral_pipeline.c
    pipeline/harmony_pipeline.c
    pipeline/form_boundary.c
    pipeline/form_pipeline.c
    pipeline/form_plan_pipeline.c
    pipeline/motive_pipeline.c
    pipeline/motive_features.c
    pipeline/melody_pipeline.c
    pipeline/progression_pipeline.c
    pipeline/harmonic_rhythm_pipeline.c
    pipeline/meter_check_pipeline.c
    pipeline/rhythm_pipeline.c
    pipeline/texture_pipeline.c
    pipeline/markings_pipeline.c
    pipeline/voice_leading_pipeline.c
    pipeline/voicing_check.c
    pipeline/counterpoint_pipeline.c
    pipeline/schema_pipeline.c
    pipeline/analysis_report.c
    pipeline/chain.c
    pipeline/analysis_pipeline.c
    pipeline/markdown_report.c
    pipeline/report_writer.c
    pipeline/path_util.c
    pipeline/analysis_document.c
    pipeline/corpus.c
    style/style_labels.c
    style/style_profile.c
    style/style_model_file.c
    style/style_compare.c
    style/similar_material.c
    links/motive_links.c
    links/motive_link_cache.c
    links/motive_links_render.c
    pipeline/composer_api.c
)
# standard-form and standard-harmony have headers of the same names (analysis_input.h, analysis_phrase.h), so the
# analyzer includes the form ones as "standard-form/analysis/...", from the frameworks directory.
get_target_property(STANDARD_FORM_SOURCE_DIR standard_form SOURCE_DIR)
get_filename_component(URSATZ_FRAMEWORKS_DIR "${STANDARD_FORM_SOURCE_DIR}/.." ABSOLUTE)
get_target_property(STANDARD_COUNTERPOINT_SOURCE_DIR standard_counterpoint SOURCE_DIR)
# standard-counterpoint's headers name each other without a directory, and its framework.h shares a name with other
# frameworks', so only the melody, voice leading and counterpoint check pipelines see its directories.
set_source_files_properties(pipeline/melody_pipeline.c pipeline/voice_leading_pipeline.c pipeline/counterpoint_pipeline.c
    PROPERTIES INCLUDE_DIRECTORIES
    "${STANDARD_COUNTERPOINT_SOURCE_DIR};${STANDARD_COUNTERPOINT_SOURCE_DIR}/analysis")
# standard-texture's headers name each other without a directory and share names (rule_pair.h, framework.h) with other
# frameworks', so only the texture pipeline sees its directories.
get_target_property(STANDARD_TEXTURE_SOURCE_DIR standard_texture SOURCE_DIR)
set_source_files_properties(pipeline/texture_pipeline.c PROPERTIES INCLUDE_DIRECTORIES
    "${STANDARD_TEXTURE_SOURCE_DIR};${STANDARD_TEXTURE_SOURCE_DIR}/analysis")
# standard-markings' headers share names (framework.h, rule_pair.h) with other frameworks', so only the markings
# pipeline sees its directory.
get_target_property(STANDARD_MARKINGS_SOURCE_DIR standard_markings SOURCE_DIR)
set_source_files_properties(pipeline/markings_pipeline.c PROPERTIES INCLUDE_DIRECTORIES
    "${STANDARD_MARKINGS_SOURCE_DIR}")
# standard-voicing's headers share names (framework.h, rule_pair.h) with other frameworks', so only the voicing check
# sees its directory.
get_target_property(STANDARD_VOICING_SOURCE_DIR standard_voicing SOURCE_DIR)
set_source_files_properties(pipeline/voicing_check.c PROPERTIES INCLUDE_DIRECTORIES "${STANDARD_VOICING_SOURCE_DIR}")
# galant-schemas and scale-harmonization name their headers without a directory (and read standard-counterpoint's), so
# only the schema pipeline sees their directories.
get_target_property(GALANT_SCHEMAS_SOURCE_DIR galant_schemas SOURCE_DIR)
get_target_property(SCALE_HARMONIZATION_SOURCE_DIR scale_harmonization SOURCE_DIR)
set_source_files_properties(pipeline/schema_pipeline.c PROPERTIES INCLUDE_DIRECTORIES
    "${GALANT_SCHEMAS_SOURCE_DIR};${GALANT_SCHEMAS_SOURCE_DIR}/analysis;${SCALE_HARMONIZATION_SOURCE_DIR};${SCALE_HARMONIZATION_SOURCE_DIR}/analysis;${STANDARD_COUNTERPOINT_SOURCE_DIR}")
configure_file(version.h.in ${PROJECT_BINARY_DIR}/include/ursatz_analyzer/version.h @ONLY)
target_include_directories(ursatz_analyzer
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include ${URSATZ_FRAMEWORKS_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ursatz_analyzer
    PUBLIC ursatz common_practice_minimal standard_meter standard_tonality standard_harmony standard_form
    PRIVATE cjson $<LINK_ONLY:standard_counterpoint> $<LINK_ONLY:standard_texture> $<LINK_ONLY:standard_voicing>
    $<LINK_ONLY:standard_markings>
    $<LINK_ONLY:galant_schemas> $<LINK_ONLY:scale_harmonization>
)

add_executable(ursatz-analyzer main.c)
target_include_directories(ursatz-analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ursatz-analyzer PRIVATE ursatz_analyzer)

add_library(ursatz_analyzer STATIC
    version.c
    pipeline/imported_notes.c
    pipeline/notation_reading.c
    pipeline/midi_pipeline.c
    pipeline/gm_names.c
    pipeline/musicxml_pipeline.c
    pipeline/input_pipeline.c
    pipeline/segmentation.c
    pipeline/chain_input.c
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
    pipeline/motive_sketch_map.c
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
    pipeline/corpus_walk.c
    pipeline/run_file.c
    style/style_labels.c
    style/style_profile.c
    style/style_model_file.c
    style/style_compare.c
    style/similar_material.c
    links/motive_links.c
    links/motive_link_cache.c
    links/motive_links_render.c
    pipeline/composer_api.c
    util/json_util.c
    util/file_util.c
    util/misc_util.c
)
configure_file(version.h.in ${PROJECT_BINARY_DIR}/include/ursatz_analyzer/version.h @ONLY)
target_include_directories(ursatz_analyzer
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ursatz_analyzer
    PUBLIC ursatz ursatz_chain common_practice_minimal standard_meter standard_tonality standard_harmony standard_form
    PRIVATE cjson standard_counterpoint standard_texture standard_voicing standard_markings galant_schemas
    scale_harmonization
)

add_executable(ursatz-analyzer main.c)
target_include_directories(ursatz-analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ursatz-analyzer PRIVATE ursatz_analyzer)

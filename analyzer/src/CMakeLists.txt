add_library(ursatz_analyzer STATIC
    pipeline/imported_notes.c
    pipeline/midi_pipeline.c
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
    pipeline/melody_pipeline.c
    pipeline/progression_pipeline.c
    pipeline/harmonic_rhythm_pipeline.c
    pipeline/rhythm_pipeline.c
    pipeline/texture_pipeline.c
    pipeline/analysis_report.c
    pipeline/chain.c
    pipeline/analysis_pipeline.c
    pipeline/markdown_report.c
    pipeline/report_writer.c
    pipeline/path_util.c
    pipeline/analysis_document.c
)
# standard-form and standard-harmony have headers of the same names (analysis_input.h, analysis_phrase.h), so the
# analyzer includes the form ones as "standard-form/analysis/...", from the frameworks directory.
get_target_property(STANDARD_FORM_SOURCE_DIR standard_form SOURCE_DIR)
get_filename_component(URSATZ_FRAMEWORKS_DIR "${STANDARD_FORM_SOURCE_DIR}/.." ABSOLUTE)
get_target_property(STANDARD_COUNTERPOINT_SOURCE_DIR standard_counterpoint SOURCE_DIR)
# standard-counterpoint's headers name each other without a directory, and its framework.h shares a name with other
# frameworks', so only the melody pipeline sees its directories.
set_source_files_properties(pipeline/melody_pipeline.c PROPERTIES INCLUDE_DIRECTORIES
    "${STANDARD_COUNTERPOINT_SOURCE_DIR};${STANDARD_COUNTERPOINT_SOURCE_DIR}/analysis")
# standard-texture's headers name each other without a directory and share names (rule_pair.h, framework.h) with other
# frameworks', so only the texture pipeline sees its directories.
get_target_property(STANDARD_TEXTURE_SOURCE_DIR standard_texture SOURCE_DIR)
set_source_files_properties(pipeline/texture_pipeline.c PROPERTIES INCLUDE_DIRECTORIES
    "${STANDARD_TEXTURE_SOURCE_DIR};${STANDARD_TEXTURE_SOURCE_DIR}/analysis")
target_include_directories(ursatz_analyzer
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${URSATZ_FRAMEWORKS_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ursatz_analyzer
    PUBLIC ursatz common_practice_minimal standard_meter standard_tonality standard_harmony standard_form
    PRIVATE cjson $<LINK_ONLY:standard_counterpoint> $<LINK_ONLY:standard_texture>
)
target_compile_definitions(ursatz_analyzer PRIVATE
    URSATZ_ANALYZER_VERSION="${PROJECT_VERSION}"
    URSATZ_ANALYZER_URSATZ_COMMIT="${URSATZ_ANALYZER_URSATZ_COMMIT}"
)

add_executable(ursatz-analyzer main.c)
target_link_libraries(ursatz-analyzer PRIVATE ursatz_analyzer)

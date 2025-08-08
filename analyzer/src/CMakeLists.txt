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
    pipeline/progression_pipeline.c
    pipeline/harmonic_rhythm_pipeline.c
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
target_include_directories(ursatz_analyzer
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${URSATZ_FRAMEWORKS_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ursatz_analyzer
    PUBLIC ursatz common_practice_minimal standard_meter standard_tonality standard_harmony standard_form
    PRIVATE cjson
)
target_compile_definitions(ursatz_analyzer PRIVATE
    URSATZ_ANALYZER_VERSION="${PROJECT_VERSION}"
    URSATZ_ANALYZER_URSATZ_COMMIT="${URSATZ_ANALYZER_URSATZ_COMMIT}"
)

add_executable(ursatz-analyzer main.c)
target_link_libraries(ursatz-analyzer PRIVATE ursatz_analyzer)

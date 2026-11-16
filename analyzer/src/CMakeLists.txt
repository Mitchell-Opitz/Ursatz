add_library(ursatz_analyzer STATIC
    version.c
    pipeline/imported_notes.c
    pipeline/notation_reading.c
    pipeline/midi_pipeline.c
    pipeline/gm_names.c
    pipeline/musicxml_pipeline.c
    pipeline/input_pipeline.c
    pipeline/chain_input.c
    pipeline/pitch_class_util.c
    pipeline/spelling_util.c
    pipeline/key_region_pipeline.c
    pipeline/chord_pipeline.c
    pipeline/outer_voice_pipeline.c
    pipeline/roman_numeral_pipeline.c
    pipeline/harmony_pipeline.c
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
    pipeline/schema_pipeline.c
    pipeline/analysis_report.c
    pipeline/chain.c
    pipeline/analysis_result.c
    pipeline/analysis_pipeline.c
    pipeline/chord_sequence.c
    pipeline/staleness.c
    pipeline/playback.c
    pipeline/score_export.c
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
    export/score_text_reader.c
    export/text_export.c
    export/material_export.c
)
configure_file(version.h.in ${PROJECT_BINARY_DIR}/include/ursatz_analyzer/version.h @ONLY)

# The producer key (R-554): a hash of the sources that produce an analysis, stored in every analysis document, corpus
# index and cache and compared by the stale check. A change to a test, a doc or the GUI leaves it as it was.
get_filename_component(URSATZ_REPO_ROOT ${PROJECT_SOURCE_DIR}/.. ABSOLUTE)
set(producer_sources "")
foreach(producer_dir IN ITEMS library/src library/frameworks library/chain analyzer/src analyzer/include third_party/cjson third_party/ezxml)
    file(GLOB_RECURSE found CONFIGURE_DEPENDS RELATIVE ${URSATZ_REPO_ROOT}
        ${URSATZ_REPO_ROOT}/${producer_dir}/*.c ${URSATZ_REPO_ROOT}/${producer_dir}/*.h)
    list(APPEND producer_sources ${found})
endforeach()
file(GLOB_RECURSE found CONFIGURE_DEPENDS RELATIVE ${URSATZ_REPO_ROOT} ${URSATZ_REPO_ROOT}/analyzer/src/*.in)
list(APPEND producer_sources ${found})
list(FILTER producer_sources EXCLUDE REGEX "/tests/")
list(SORT producer_sources)
list(TRANSFORM producer_sources PREPEND "${URSATZ_REPO_ROOT}/" OUTPUT_VARIABLE producer_source_files)
string(REPLACE ";" "\n" producer_list_text "${producer_sources}")
set(producer_list ${PROJECT_BINARY_DIR}/producer_sources.txt)
file(CONFIGURE OUTPUT ${producer_list} CONTENT "${producer_list_text}\n" @ONLY)
set(producer_key_header ${PROJECT_BINARY_DIR}/include/ursatz_analyzer/producer_key.h)
add_custom_command(
    OUTPUT ${producer_key_header}
    COMMAND ${CMAKE_COMMAND} -DROOT=${URSATZ_REPO_ROOT} -DLIST=${producer_list} -DOUT=${producer_key_header}
            -P ${PROJECT_SOURCE_DIR}/cmake/ProducerKey.cmake
    DEPENDS ${producer_source_files} ${producer_list} ${PROJECT_SOURCE_DIR}/cmake/ProducerKey.cmake
    COMMENT "Computing the Analyzer producer key"
    VERBATIM)
target_sources(ursatz_analyzer PRIVATE ${producer_key_header})
target_include_directories(ursatz_analyzer
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ursatz_analyzer
    PUBLIC ursatz
    PRIVATE ursatz_chain common_practice_minimal standard_meter standard_tonality standard_harmony standard_form cjson
    standard_counterpoint standard_texture standard_voicing standard_markings galant_schemas scale_harmonization
)

# Reading the chain's claims (analyzer_result_chain): the chain headers reach only a consumer that links this target.
add_library(ursatz_analyzer_chain INTERFACE)
add_library(ursatz_analyzer::chain ALIAS ursatz_analyzer_chain)
target_link_libraries(ursatz_analyzer_chain INTERFACE ursatz_analyzer ursatz_chain)

add_executable(ursatz-analyzer main.c)
target_include_directories(ursatz-analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ursatz-analyzer PRIVATE ursatz_analyzer)

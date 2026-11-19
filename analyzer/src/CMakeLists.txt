add_library(ursatz_analyzer STATIC
    version.c
    result/input_pipeline.c
    result/analysis_result.c
    corpus/chord_sequence.c
    corpus/corpus_passages.c
    store/staleness.c
    export/playback.c
    export/score_export.c
    store/path_util.c
    export/analysis_document.c
    export/document_shared.c
    export/document_keys.c
    export/document_chords.c
    export/document_harmony.c
    export/document_form.c
    export/document_motive.c
    export/document_rhythm.c
    export/document_voice_leading.c
    export/document_print.c
    corpus/corpus.c
    store/corpus_walk.c
    result/run_file.c
    style/style_profile.c
    style/style_piece_document.c
    corpus/similar_material.c
    links/motive_links.c
    links/motive_link_cache.c
    links/motive_links_render.c
    util/misc_util.c
    export/score_text.c
    export/text_export.c
    export/material_export.c
)
configure_file(version.h.in ${PROJECT_BINARY_DIR}/include/ursatz_analyzer/version.h @ONLY)

# The producer key (R-554): a hash of the sources that produce an analysis, stored in every analysis document, corpus
# index and cache and compared by the stale check. A change to a test, a doc or the GUI leaves it as it was.
get_filename_component(URSATZ_REPO_ROOT ${PROJECT_SOURCE_DIR}/.. ABSOLUTE)
set(producer_sources "")
foreach(producer_dir IN ITEMS library/model library/theory library/analysis library/io library/chain analyzer/src analyzer/include third_party/cjson third_party/ezxml)
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
            -DMACRO=URSATZ_ANALYZER_PRODUCER_KEY -P ${URSATZ_REPO_ROOT}/cmake/ProducerKey.cmake
    DEPENDS ${producer_source_files} ${producer_list} ${URSATZ_REPO_ROOT}/cmake/ProducerKey.cmake
    COMMENT "Computing the Analyzer producer key"
    VERBATIM)
target_sources(ursatz_analyzer PRIVATE ${producer_key_header})
target_include_directories(ursatz_analyzer
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ursatz_analyzer
    PUBLIC ursatz_io ursatz_analysis ursatz_chain ursatz_realization ursatz_reading ursatz_plan ursatz_common
    PRIVATE cjson
)

add_executable(ursatz-analyzer cli/main.c)
target_include_directories(ursatz-analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ursatz-analyzer PRIVATE ursatz_analyzer)

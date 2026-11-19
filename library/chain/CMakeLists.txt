# Analysis chain: an extension above the model, theory and analysis groups that joins them
# (R-241). Sources are added as the chain grows.
add_library(ursatz_chain
    analysis_chain.c
    chain_inputs_common.c
    chain_inputs_segments.c
    chain_inputs_keys.c
    chain_inputs_outer_voices.c
    chain_inputs_cadences.c
    chain_inputs_form.c
    chain_inputs_meter.c
    chain_inputs_texture.c
    chain_inputs_voice_leading.c
    chain_inputs_melody.c
    chain_inputs_schemas.c
    form_bridge.c
    kernel_owners.c
    chain_result.c
    chain_collect.c
    chain_motive_sketch.c
    chain_result_chords.c
    chain_result_form.c
    chain_result_form_plan.c
    chain_result_harmonic_rhythm.c
    chain_result_harmony.c
    chain_result_key_regions.c
    chain_result_markings.c
    chain_result_melody.c
    chain_result_meter_check.c
    chain_result_motive.c
    chain_result_motive_features.c
    chain_result_numerals.c
    chain_result_outer_voices.c
    chain_result_progressions.c
    chain_result_rhythm.c
    chain_result_schemas.c
    chain_result_spelling.c
    chain_result_texture.c
    chain_result_voice_faults.c
    chain_result_voice_leading.c
    chain_result_voicing_check.c
    chain_score_input.c
    chain_style.c
    chain_producer_key.c
)

# The library's producer key (R-620): a hash of the sources that produce an analysis in the library (model, theory, analysis, io and
# chain, and the vendored cJSON and ezxml), stamped in the plans and versions the Composer writes (analysis_chain_producer_key). A
# change to a test, a doc or an app leaves it as it was. The Analyzer's own key (analyzer/src/CMakeLists.txt) also covers its sources.
get_filename_component(URSATZ_REPO_ROOT ${PROJECT_SOURCE_DIR}/.. ABSOLUTE)
set(chain_key_sources "")
foreach(key_dir IN ITEMS library/model library/theory library/analysis library/io library/chain third_party/cjson third_party/ezxml)
    file(GLOB_RECURSE found CONFIGURE_DEPENDS RELATIVE ${URSATZ_REPO_ROOT}
        ${URSATZ_REPO_ROOT}/${key_dir}/*.c ${URSATZ_REPO_ROOT}/${key_dir}/*.h)
    list(APPEND chain_key_sources ${found})
endforeach()
list(FILTER chain_key_sources EXCLUDE REGEX "/tests/")
list(SORT chain_key_sources)
list(TRANSFORM chain_key_sources PREPEND "${URSATZ_REPO_ROOT}/" OUTPUT_VARIABLE chain_key_files)
string(REPLACE ";" "\n" chain_key_text "${chain_key_sources}")
set(chain_key_list ${PROJECT_BINARY_DIR}/chain_producer_sources.txt)
file(CONFIGURE OUTPUT ${chain_key_list} CONTENT "${chain_key_text}\n" @ONLY)
set(chain_key_header ${PROJECT_BINARY_DIR}/chain_generated/chain/producer_key.h)
add_custom_command(
    OUTPUT ${chain_key_header}
    COMMAND ${CMAKE_COMMAND} -DROOT=${URSATZ_REPO_ROOT} -DLIST=${chain_key_list} -DOUT=${chain_key_header}
            -DMACRO=URSATZ_CHAIN_PRODUCER_KEY -P ${URSATZ_REPO_ROOT}/cmake/ProducerKey.cmake
    DEPENDS ${chain_key_files} ${chain_key_list} ${URSATZ_REPO_ROOT}/cmake/ProducerKey.cmake
    COMMENT "Computing the library producer key"
    VERBATIM)
target_sources(ursatz_chain PRIVATE ${chain_key_header})
target_include_directories(ursatz_chain PRIVATE ${PROJECT_BINARY_DIR}/chain_generated)
# Public headers (R-223), staged under chain/ in the build tree; the chain directory is private.
ursatz_public_headers(ursatz_chain
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
    HEADERS
        chain/analysis_chain.h
        chain/chain_inputs.h
        chain/chain_result.h
        chain/chain_motive_sketch.h
        chain/chain_style.h
)
target_link_libraries(ursatz_chain PUBLIC ursatz_analysis ursatz_io)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

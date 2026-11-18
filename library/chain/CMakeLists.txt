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
    chain_result_voice_leading.c
    chain_result_voicing_check.c
    chain_score_input.c
    chain_style.c
)
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

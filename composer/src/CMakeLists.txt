add_library(ursatz_composer_engine STATIC
    composer.c
    structure/request_parse.c
    piece/request_chords.c
    api/api_internal.c
    api/api_versions.c
    api/api_export.c
    api/api_motive_request.c
    api/api_motive.c
    api/api_variants.c
    api/api_random.c
    api/api_construction.c
    api/api_grow.c
    api/api_grow_json.c
    api/api_seed.c
    api/api_style.c
    api/api_theme.c
    api/api_critique.c
    api/api_target.c
    api/api_workflow.c
    api/api_plan.c
    api/api_fill.c
    api/api_regenerate.c
    api/api_variations.c
    api/api_piece_request.c
    api/api_piece.c
    api/api_piece_compose.c
    api/api_piece_json.c
    api/api_piece_output.c
    api/api_piece_regenerate.c
    api/api_piece_rows.c
    api/api_piece_variations.c
    api/api_palette.c
    api/api_progression.c
    api/api_progression_lab.c
    api/api_counterpoint.c
    api/api_develop.c
    api/api_grow_request.c
    api/api_vocabulary.c
    api/api_forms.c
    api/api_validate.c
    util/job.c
    util/util.c
    structure/plan_util.c
    style/style.c
    style/style_blend.c
    style/style_labels.c
    check/style_check.c
    check/style_measure.c
    api/corpus_seed.c
    check/analyzer_check.c
    check/hearing.c
    check/key_hear.c
    critic/critic_line.c
    critic/critic_evaluate.c
    critic/critic_observe.c
    explain/explain.c
    render/render.c
    render/movement_build.c
    render/export.c
    structure/fill_target.c
    fill/fill_operator.c
    fill/fill_loop.c
    fill/fill_rng.c
    fill/candidate.c
    fill/generation_record.c
    fill/fill_record.c
    fill/fill_suggest.c
    candidates/variations.c
    harmony/harmony_palette.c
    harmony/harmony_roman.c
    harmony/harmony_mask.c
    harmony/harmony_rhythm.c
    harmony/harmony_request.c
    harmony/harmony_progression.c
    harmony/harmony_chords.c
    harmony/harmony_hear.c
    harmony/harmony_check.c
    harmony/harmony_phrase.c
    harmony/harmony_prolong.c
    harmony/harmony_context.c
    harmony/harmony_given.c
    harmony/harmony_motive.c
    harmony/harmony_templates.c
    harmony/harmony_embellish.c
    harmony/harmony_bass.c
    harmony/harmony_bass_check.c
    harmony/harmony_skeleton.c
    harmony/harmony_schema.c
    harmony/harmony_sequence.c
    motive/motive.c
    motive/motive_notes.c
    motive/motive_text.c
    motive/motive_render.c
    motive/motive_import.c
    motive/motive_analysis.c
    motive/motive_segments.c
    motive/motive_develop.c
    motive/motive_develop_operator.c
    motive/motive_recognizability.c
    motive/motive_fidelity.c
    motive/motive_invent.c
    motive/motive_random.c
    motive/motive_random_common.c
    motive/motive_random_layout.c
    motive/motive_random_pitch.c
    motive/motive_random_rhythm.c
    motive/motive_random_segments.c
    motive/motive_random_blueprint.c
    motive/variant_registry.c
    motive/variant_chain.c
    motive/variant_functions.c
    motive/variant_recognize.c
    motive/motive_tones.c
    motive/motive_blueprint.c
    motive/motive_connect.c
    melody/melody_check.c
    melody/melody_context.c
    melody/melody_contour.c
    melody/melody_nct.c
    melody/melody_place.c
    melody/melody_realize.c
    melody/melody_fill.c
    preferences/preferences.c
    preferences/preferences_development.c
    preferences/preferences_critic.c
    preferences/preferences_harmony.c
    preferences/preferences_blueprint.c
    preferences/preferences_preset.c
    check/form_hear.c
    structure/form_record.c
    structure/form_spec.c
    structure/form_builtin.c
    check/return_hear.c
    grow/grow_blueprint.c
    grow/grow_blueprint_generate.c
    grow/grow_request.c
    grow/grow_apply.c
    grow/grow_contrast.c
    grow/grow_draw.c
    grow/grow_idea.c
    grow/grow_theme.c
    grow/grow_generate.c
    grow/grow_layout.c
    structure/theme_shape.c
    grow/grow_develop.c
    grow/grow_fidelity.c
    grow/grow_segments.c
    grow/grow_beginning.c
    grow/grow_ending.c
    grow/grow_function_harmony.c
    grow/grow_free.c
    grow/grow_liquidate.c
    grow/grow_coda.c
    grow/grow_check.c
    grow/grow_form.c
    piece/grow_run.c
    material/form_return.c
    material/given.c
    material/seam.c
    piece/piece_reopen.c
    harmony/seam_hear.c
    material/connective.c
    piece/section_harmony.c
    piece/piece_perform.c
    material/form_vary.c
    material/form_figure.c
    piece/form_build.c
    piece/form_build_request.c
    piece/piece_keys.c
    piece/piece_cadences.c
    piece/compose_run.c
    candidates/compose_candidates.c
    structure/lock.c
    structure/depend.c
    piece/regenerate.c
    texture/accompaniment.c
    texture/accompaniment_pattern.c
    texture/accompaniment_hear.c
    texture/voicing.c
    texture/voicing_check.c
    texture/voicing_search.c
    piece/texture_check.c
    counterpoint/counterpoint_write.c
    counterpoint/counterpoint_species.c
    counterpoint/counterpoint_check.c
)
# The forms built in (R-416): forms/<name>.json, embedded as C byte arrays at build time. Adding a form is a file in
# forms/ and a line here, in registry order.
set(COMPOSER_FORMS binary rounded-binary ternary variations loop layered minuet trio minuet-and-trio scherzo scherzo-and-trio figured-variations main-and-subordinate andante-aba andante-abab small-rondo-ababa small-rondo-abaca large-rondo sonata-rondo sonatina sonatina-tonic-major game-loop)
set(COMPOSER_FORM_FILES)
foreach(form ${COMPOSER_FORMS})
    list(APPEND COMPOSER_FORM_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../forms/${form}.json)
endforeach()
set(COMPOSER_FORM_DATA ${CMAKE_CURRENT_BINARY_DIR}/form/form_builtin_data.c)
add_custom_command(
    OUTPUT ${COMPOSER_FORM_DATA}
    COMMAND ${CMAKE_COMMAND} -DOUTPUT=${COMPOSER_FORM_DATA} "-DFILES=${COMPOSER_FORM_FILES}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/EmbedForms.cmake
    DEPENDS ${COMPOSER_FORM_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/EmbedForms.cmake
    COMMENT "Embedding the form documents"
    VERBATIM)
target_sources(ursatz_composer_engine PRIVATE ${COMPOSER_FORM_DATA})
# The presets built in (R-754): presets/<name>.json, embedded the same way. Adding a preset is a file in presets/ and a name here.
set(COMPOSER_PRESETS ff7 hollow-knight)
set(COMPOSER_PRESET_FILES)
foreach(preset ${COMPOSER_PRESETS})
    list(APPEND COMPOSER_PRESET_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../presets/${preset}.json)
endforeach()
set(COMPOSER_PRESET_DATA ${CMAKE_CURRENT_BINARY_DIR}/preset/preset_builtin_data.c)
add_custom_command(
    OUTPUT ${COMPOSER_PRESET_DATA}
    COMMAND ${CMAKE_COMMAND} -DOUTPUT=${COMPOSER_PRESET_DATA} "-DFILES=${COMPOSER_PRESET_FILES}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/EmbedPresets.cmake
    DEPENDS ${COMPOSER_PRESET_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/EmbedPresets.cmake
    COMMENT "Embedding the preset documents"
    VERBATIM)
target_sources(ursatz_composer_engine PRIVATE ${COMPOSER_PRESET_DATA})
# The Composer's composition key (R-626): a hash of the sources that compose a piece (the Composer's sources and forms, and the
# library groups it is made of, with the vendored cJSON), stamped in every plan the Composer writes (composer_composition_key).
# A change to a test, a doc or an app leaves it as it was.
get_filename_component(URSATZ_REPO_ROOT ${PROJECT_SOURCE_DIR}/.. ABSOLUTE)
set(composition_sources "")
foreach(key_dir IN ITEMS composer/src library/model library/theory library/realization library/plan library/analysis library/io library/chain third_party/cjson)
    file(GLOB_RECURSE found CONFIGURE_DEPENDS RELATIVE ${URSATZ_REPO_ROOT}
        ${URSATZ_REPO_ROOT}/${key_dir}/*.c ${URSATZ_REPO_ROOT}/${key_dir}/*.h)
    list(APPEND composition_sources ${found})
endforeach()
file(GLOB found CONFIGURE_DEPENDS RELATIVE ${URSATZ_REPO_ROOT} ${URSATZ_REPO_ROOT}/composer/forms/*.json)
list(APPEND composition_sources ${found})
list(FILTER composition_sources EXCLUDE REGEX "/tests/")
list(SORT composition_sources)
list(TRANSFORM composition_sources PREPEND "${URSATZ_REPO_ROOT}/" OUTPUT_VARIABLE composition_key_files)
string(REPLACE ";" "\n" composition_key_text "${composition_sources}")
set(composition_key_list ${PROJECT_BINARY_DIR}/composition_sources.txt)
file(CONFIGURE OUTPUT ${composition_key_list} CONTENT "${composition_key_text}\n" @ONLY)
set(composition_key_header ${PROJECT_BINARY_DIR}/composer_generated/composer/composition_key.h)
add_custom_command(
    OUTPUT ${composition_key_header}
    COMMAND ${CMAKE_COMMAND} -DROOT=${URSATZ_REPO_ROOT} -DLIST=${composition_key_list} -DOUT=${composition_key_header}
            -DMACRO=URSATZ_COMPOSER_COMPOSITION_KEY -P ${URSATZ_REPO_ROOT}/cmake/ProducerKey.cmake
    DEPENDS ${composition_key_files} ${composition_key_list} ${URSATZ_REPO_ROOT}/cmake/ProducerKey.cmake
    COMMENT "Computing the Composer composition key"
    VERBATIM)
target_sources(ursatz_composer_engine PRIVATE ${composition_key_header})
target_include_directories(ursatz_composer_engine
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/composer_generated
)
target_link_libraries(ursatz_composer_engine
    PUBLIC ursatz_common
    PRIVATE ursatz_io ursatz_realization ursatz_plan ursatz_reading ursatz_analysis ursatz_chain cjson
)
# The library consumers link exports only the API (exports.txt); the tests, the command line and the other code of the Composer's own use
# the internals, so they link the engine.
ursatz_localize_exports(ursatz_composer ENGINE ursatz_composer_engine KEEP ${CMAKE_CURRENT_SOURCE_DIR}/exports.txt)

# The tests reach the internal headers and the libraries the sources use; no consumer does.
add_library(ursatz_composer_internal INTERFACE)
target_include_directories(ursatz_composer_internal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ursatz_composer_internal INTERFACE
    ursatz_composer_engine
    ursatz_io ursatz_realization ursatz_plan ursatz_reading ursatz_analysis ursatz_chain ursatz_common cjson
)
target_compile_definitions(ursatz_composer_engine PRIVATE
    URSATZ_COMPOSER_VERSION="${PROJECT_VERSION}"
)
target_compile_options(ursatz_composer_engine PRIVATE -Wall -Wextra -Wpedantic)

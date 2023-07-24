add_library(ursatz_composer STATIC
    composer.c
    plan/piece_plan.c
    plan/lanes.c
    plan/plan_state.c
    plan/constraint.c
    plan/plan_json.c
    plan/analysis_load.c
    render/render.c
    render/export.c
    fill/fill_target.c
    fill/fill_operator.c
    fill/fill_loop.c
    fill/fill_rng.c
    fill/fill_record.c
    fill/fill_suggest.c
    harmony/harmony_roman.c
    harmony/harmony_rhythm.c
    harmony/harmony_request.c
    harmony/harmony_progression.c
    harmony/harmony_hear.c
    harmony/harmony_check.c
    harmony/harmony_phrase.c
)
target_include_directories(ursatz_composer
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ursatz_composer
    PUBLIC ursatz ursatz_analyzer standard_harmony
    PRIVATE cjson
)
target_compile_definitions(ursatz_composer PRIVATE
    URSATZ_COMPOSER_VERSION="${PROJECT_VERSION}"
)
target_compile_options(ursatz_composer PRIVATE -Wall -Wextra -Wpedantic)

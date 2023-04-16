add_library(ursatz_composer STATIC
    composer.c
    plan/piece_plan.c
    plan/lanes.c
    plan/plan_state.c
    plan/constraint.c
)
target_include_directories(ursatz_composer
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ursatz_composer
    PUBLIC ursatz ursatz_analyzer
    PRIVATE cjson
)
target_compile_definitions(ursatz_composer PRIVATE
    URSATZ_COMPOSER_VERSION="${PROJECT_VERSION}"
)
target_compile_options(ursatz_composer PRIVATE -Wall -Wextra -Wpedantic)

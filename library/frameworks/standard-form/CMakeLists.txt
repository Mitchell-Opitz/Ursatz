add_library(standard_form
    framework.c
    rule_pair.c
    motive_rules.c
    hierarchy_rules.c
    phrase_rules.c
    segmentation_rules.c
    sentence_rules.c
    period_rules.c
    combination_rules.c
    unique_rules.c
    beginning_rules.c
    ending_rules.c
    cadential_bass_rules.c
    hybrid_rules.c
    length_rules.c
    internal_expansion_rules.c
)
target_include_directories(standard_form PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_form PUBLIC ursatz standard_harmony)

ursatz_add_tests()

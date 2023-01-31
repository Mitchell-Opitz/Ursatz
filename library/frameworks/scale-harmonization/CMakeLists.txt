add_library(scale_harmonization
    scale_framework.c
    scale_rule_pair.c
    scale_rule_table.c
    scale_harmonization.c
    scale_patterns.c
    scale_sequence_rules.c
    scale_octave_rules.c
)
target_include_directories(scale_harmonization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/frameworks
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(scale_harmonization PUBLIC galant_schemas standard_counterpoint ursatz)

ursatz_add_tests()

add_library(galant_schemas
    galant_framework.c
    galant_rule_pair.c
    galant_rule_table.c
    galant_degree.c
    galant_exemplar.c
    galant_prototypes.c
    galant_matching_rules.c
    galant_classification.c
    analysis/galant_analysis_input.c
    analysis/galant_analysis_claim.c
    analysis/galant_analysis_match.c
    schema_function.c
)
target_include_directories(galant_schemas PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    ${PROJECT_SOURCE_DIR}/frameworks
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(galant_schemas PUBLIC standard_counterpoint ursatz)

ursatz_add_tests()

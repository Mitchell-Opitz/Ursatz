add_library(galant_schemas
    galant_framework.c
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
# Public headers (R-223), staged under galant-schemas/ in the build tree; the source directories are private.
ursatz_public_headers(galant_schemas
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX galant-schemas
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/galant_analysis_input.h
        analysis/galant_analysis_match.h
        galant_degree.h
        galant_exemplar.h
        galant_framework.h
        galant_prototype.h
        galant_prototypes.h
        schema_function.h
)
target_link_libraries(galant_schemas PUBLIC standard_counterpoint ursatz)

ursatz_add_tests()

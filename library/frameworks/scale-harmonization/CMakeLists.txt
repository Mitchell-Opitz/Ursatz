add_library(scale_harmonization
    scale_framework.c
    scale_rule_table.c
    scale_harmonization.c
    scale_patterns.c
    scale_sequence_rules.c
    scale_octave_rules.c
    analysis/scale_analysis_claim.c
    analysis/scale_analysis_detect.c
    scale_direction.c
    sequence_kind.c
)
# Public headers (R-223), staged under scale-harmonization/ in the build tree; the source directories are private.
ursatz_public_headers(scale_harmonization
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX scale-harmonization
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/scale_analysis_detect.h
        scale_direction.h
        scale_framework.h
        scale_patterns.h
        sequence_kind.h
)
target_link_libraries(scale_harmonization PUBLIC galant_schemas standard_counterpoint ursatz)

ursatz_add_tests()

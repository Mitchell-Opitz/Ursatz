add_library(sixteenth_century_style
    century_framework.c
    century_rule_table.c
    century_score.c
    century_melodic_rules.c
    century_harmonic_rules.c
    century_dissonance_rules.c
    century_rhythm_rules.c
    century_imitation_rules.c
    century_rules.c
)
# No app, chain or framework source includes a header of sixteenth-century-style yet, so nothing is public (R-223).
ursatz_public_headers(sixteenth_century_style
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX sixteenth-century-style
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(sixteenth_century_style PUBLIC standard_counterpoint ursatz)

ursatz_add_tests()

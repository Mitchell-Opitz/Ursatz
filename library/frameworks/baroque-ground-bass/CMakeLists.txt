add_library(baroque_ground_bass
    ground_framework.c
    ground_rule_table.c
    ground_piece.c
    ground_bass_rules.c
    ground_variety_rules.c
    ground_imitation_rules.c
    ground_rules.c
    imitative_role.c
    ostinato_type.c
)
# No app, chain or framework source includes a header of baroque-ground-bass yet, so nothing is public (R-223).
ursatz_public_headers(baroque_ground_bass
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX baroque-ground-bass
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(baroque_ground_bass PUBLIC standard_counterpoint ursatz)

ursatz_add_tests()

add_library(high_baroque_fugue
    fugue_framework.c
    fugue_rule_table.c
    fugue_exposition.c
    fugue_entry_rules.c
    fugue_answer_rules.c
    fugue_countersubject_rules.c
    fugue_rules.c
    fugue_role.c
)
# No app, chain or framework source includes a header of high-baroque-fugue yet, so nothing is public (R-223).
ursatz_public_headers(high_baroque_fugue
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX high-baroque-fugue
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(high_baroque_fugue PUBLIC standard_counterpoint ursatz)

ursatz_add_tests()

add_library(standard_notation
    framework.c
    rule_pair.c
    staff_clef_rules.c
    ledger_line_rules.c
    stem_rules.c
    second_rules.c
)
target_include_directories(standard_notation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_notation PUBLIC ursatz)

add_library(high_baroque_fugue
    fugue_framework.c
    fugue_rule_pair.c
    fugue_rule_table.c
    fugue_exposition.c
    fugue_entry_rules.c
    fugue_answer_rules.c
    fugue_countersubject_rules.c
    fugue_rules.c
)
target_include_directories(high_baroque_fugue PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/frameworks
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(high_baroque_fugue PUBLIC standard_counterpoint ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

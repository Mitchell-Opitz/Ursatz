add_library(baroque_ground_bass
    ground_framework.c
    ground_rule_pair.c
    ground_rule_table.c
    ground_piece.c
    ground_bass_rules.c
)
target_include_directories(baroque_ground_bass PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/frameworks
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(baroque_ground_bass PUBLIC standard_counterpoint ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

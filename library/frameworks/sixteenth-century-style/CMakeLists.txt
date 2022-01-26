add_library(sixteenth_century_style
    century_framework.c
    century_rule_pair.c
    century_rule_table.c
    century_score.c
)
target_include_directories(sixteenth_century_style PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/frameworks
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(sixteenth_century_style PUBLIC standard_counterpoint ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

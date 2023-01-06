add_library(standard_form
    framework.c
    rule_pair.c
    motive_rules.c
    hierarchy_rules.c
    phrase_rules.c
    segmentation_rules.c
    sentence_rules.c
    period_rules.c
    combination_rules.c
    unique_rules.c
    beginning_rules.c
    ending_rules.c
    cadential_bass_rules.c
)
target_include_directories(standard_form PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_form PUBLIC ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

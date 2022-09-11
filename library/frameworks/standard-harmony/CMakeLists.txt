add_library(standard_harmony
    harmony_framework.c
    harmony_rule_pair.c
    harmony_rule_table.c
    harmony_chord.c
    harmony_functions.c
    harmony_phrase_rules.c
    harmony_phrase_labels.c
    harmony_phrase_label_rules.c
    harmony_cadence_rules.c
    harmony_hearing_rules.c
    harmony_writing_rules.c
    harmony_seventh_rules.c
    harmony_strong_predominants.c
    harmony_steps.c
    harmony_predominant_rules.c
    harmony_embellishing.c
    harmony_embellishing_rules.c
    harmony_cadential_rules.c
    harmony_prolongation.c
    harmony_prolongation_rules.c
    harmony_prolongation_chain.c
    harmony_six_four.c
    harmony_passing_rules.c
)
target_include_directories(standard_harmony PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/frameworks
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_harmony PUBLIC ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

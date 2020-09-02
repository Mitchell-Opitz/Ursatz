add_library(standard_notation
    framework.c
    rule_pair.c
    staff_clef_rules.c
    ledger_line_rules.c
    stem_rules.c
    beam_stem_rules.c
    flag_rules.c
    second_rules.c
    octave_equivalence_rules.c
    clef_letter_rules.c
    semitone.c
    step_rules.c
    accidental_rules.c
    accidental_placement_rules.c
    enharmonic_rules.c
)
target_include_directories(standard_notation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_notation PUBLIC ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

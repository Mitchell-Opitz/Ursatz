add_library(standard_tonality
    framework.c
    rule_pair.c
    tonal_pitch.c
    scale_pattern.c
    major_scale_rules.c
    minor_scale_rules.c
    mode_rules.c
    pentatonic_rules.c
    chromatic_scale_rules.c
    interval_size_rules.c
    interval_quality_rules.c
    interval_inversion_rules.c
    interval_consonance_rules.c
    interval_white_key_rules.c
    interval_enharmonic_rules.c
    interval_spelling_rules.c
    triad_rules.c
    triad_spelling_rules.c
    triad_identification_rules.c
    chord_symbol_rules.c
    triad_in_key_rules.c
    modal_solfege_rules.c
    solfege_rules.c
    scale_degree_rules.c
    minor_scale_degree_rules.c
    key_relationship_rules.c
    minor_key_rules.c
    key_signature_rules.c
    circle_of_fifths_rules.c
    seventh_rules.c
    seventh_spelling_rules.c
    seventh_identification_rules.c
)
target_include_directories(standard_tonality PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_tonality PUBLIC ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

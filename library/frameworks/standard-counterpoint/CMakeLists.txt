add_library(standard_counterpoint
    framework.c
    rule_pair.c
    counterpoint_pitch.c
    counterpoint_interval.c
    consonance_rules.c
    counterpoint_line.c
    motion_rules.c
    cantus_firmus_rules.c
    melodic_rules.c
    harmonic_rules.c
    tendency_rules.c
    counterpoint_rules.c
    first_species_harmony_rules.c
    first_species_ends_rules.c
    first_species_independence_rules.c
    first_species_motion_rules.c
    first_species_line_rules.c
    first_species_rules.c
    second_species_exercise.c
    second_species_pattern.c
    second_species_strong_rules.c
    second_species_weak_rules.c
    second_species_ends_rules.c
    second_species_line_rules.c
    second_species_rules.c
    third_species_exercise.c
)
target_include_directories(standard_counterpoint PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_counterpoint PUBLIC ursatz)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()

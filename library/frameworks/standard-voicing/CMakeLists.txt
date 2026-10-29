add_library(standard_voicing
    framework.c
    voicing_pitch.c
    voicing_rules.c
    stem_rules.c
    construction_rules.c
    range_rules.c
    spacing_rules.c
    crossing_rules.c
    doubling_rules.c
)
target_include_directories(standard_voicing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_voicing PUBLIC ursatz)

ursatz_add_tests()

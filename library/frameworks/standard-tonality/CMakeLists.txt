add_library(standard_tonality
    framework.c
    rule_pair.c
    tonal_pitch.c
    scale_pattern.c
    major_scale_rules.c
    minor_scale_rules.c
    scale_degree_rules.c
    minor_scale_degree_rules.c
    key_signature_rules.c
    circle_of_fifths_rules.c
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

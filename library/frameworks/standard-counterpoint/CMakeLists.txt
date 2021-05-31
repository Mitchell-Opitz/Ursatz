add_library(standard_counterpoint
    framework.c
    rule_pair.c
    counterpoint_pitch.c
    counterpoint_interval.c
    consonance_rules.c
    counterpoint_line.c
    motion_rules.c
    cantus_firmus_rules.c
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

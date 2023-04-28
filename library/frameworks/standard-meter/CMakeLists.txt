add_library(standard_meter
    framework.c
    rule_pair.c
    grouping_rules.c
    time_signature_rules.c
    meter_class_rules.c
    measure_rules.c
    anacrusis_rules.c
    counting_rules.c
    borrowed_rules.c
    hypermeter_rules.c
    syncopation_rules.c
    metric_position_rules.c
    beat_grid.c
    beaming_rules.c
    beat_tie_rules.c
)
target_include_directories(standard_meter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_meter PUBLIC ursatz)

ursatz_add_tests()

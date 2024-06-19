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
    hemiola_rules.c
    metric_position_rules.c
    segmentation_beat_grid.c
    beat_grid.c
    beaming_rules.c
    beat_tie_rules.c
    analysis/meter_analysis_time.c
    analysis/meter_analysis_input.c
    analysis/meter_analysis_claim.c
    analysis/meter_analysis_syncopation.c
    analysis/meter_analysis_hemiola.c
)
target_include_directories(standard_meter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(standard_meter PUBLIC ursatz)

ursatz_add_tests()

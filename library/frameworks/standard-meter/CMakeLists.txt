add_library(standard_meter
    framework.c
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
    analysis/meter_analysis_hypermeter.c
    analysis/meter_analysis_density.c
    analysis/meter_analysis_check.c
    analysis/meter_analysis_tuplet.c
)
# Public headers (R-223), staged under standard-meter/ in the build tree; the source directories are private.
ursatz_public_headers(standard_meter
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-meter
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/meter_analysis_check.h
        analysis/meter_analysis_density.h
        analysis/meter_analysis_hemiola.h
        analysis/meter_analysis_hypermeter.h
        analysis/meter_analysis_input.h
        analysis/meter_analysis_syncopation.h
        analysis/meter_analysis_time.h
        analysis/meter_analysis_tuplet.h
        beat_grid.h
        borrowed_rules.h
        framework.h
        grouping_rules.h
        meter_class_rules.h
        metric_position_rules.h
        segmentation_beat_grid.h
)
target_link_libraries(standard_meter PUBLIC ursatz)

ursatz_add_tests()

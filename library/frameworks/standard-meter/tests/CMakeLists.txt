ursatz_test_includes()

ursatz_test(standard_meter_framework SOURCES test_framework.c LIBS standard_meter)

ursatz_test(standard_meter_meter_class_rules SOURCES test_meter_class_rules.c LIBS standard_meter)

ursatz_test(standard_meter_compound_meter SOURCES test_compound_meter.c LIBS standard_meter)

ursatz_test(standard_meter_borrowed_rules SOURCES test_borrowed_rules.c LIBS standard_meter)

ursatz_test(standard_meter_metric_position_rules SOURCES test_metric_position_rules.c LIBS standard_meter)

ursatz_test(standard_meter_metric_position_integration SOURCES test_metric_position_integration.c LIBS standard_meter
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus")

ursatz_test(standard_meter_segmentation_beat_grid SOURCES test_segmentation_beat_grid.c LIBS standard_meter)

ursatz_test(standard_meter_adaptive_segmentation_eval SOURCES test_adaptive_segmentation_eval.c LIBS standard_meter ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations")

ursatz_test(standard_meter_hemiola_rules SOURCES test_hemiola_rules.c LIBS standard_meter)

ursatz_test(standard_meter_syncopation_through_stronger SOURCES test_syncopation_through_stronger.c
    LIBS standard_meter)

ursatz_test(standard_meter_analysis_syncopation SOURCES test_analysis_syncopation.c LIBS standard_meter)

ursatz_test(standard_meter_analysis_hemiola SOURCES test_analysis_hemiola.c LIBS standard_meter)

ursatz_test(standard_meter_hypermeasure_rules SOURCES test_hypermeasure_rules.c LIBS standard_meter)

ursatz_test(standard_meter_analysis_hypermeter SOURCES test_analysis_hypermeter.c LIBS standard_meter)

ursatz_test(standard_meter_analysis_density SOURCES test_analysis_density.c LIBS standard_meter)

ursatz_test(standard_meter_analysis_note SOURCES test_meter_analysis_note.c LIBS standard_meter VALGRIND)

ursatz_test(standard_meter_analysis_check SOURCES test_meter_analysis_check.c LIBS standard_meter VALGRIND)

ursatz_test(standard_meter_analysis_tuplet SOURCES test_meter_analysis_tuplet.c LIBS standard_meter VALGRIND)

ursatz_test(standard_meter_accent_beats SOURCES test_accent_beats.c LIBS standard_meter VALGRIND)


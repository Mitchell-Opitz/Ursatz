ursatz_test(standard_meter_framework SOURCES test_framework.c LIBS ursatz_theory)

ursatz_test(standard_meter_meter_class_rules SOURCES test_meter_class_rules.c LIBS ursatz_theory)

ursatz_test(standard_meter_compound_meter SOURCES test_compound_meter.c LIBS ursatz_theory)

ursatz_test(standard_meter_borrowed_rules SOURCES test_borrowed_rules.c LIBS ursatz_theory)

ursatz_test(standard_meter_metric_position_rules SOURCES test_metric_position_rules.c LIBS ursatz_theory)

ursatz_test(standard_meter_metric_position_integration SOURCES test_metric_position_integration.c LIBS ursatz_theory ursatz
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus")

ursatz_test(standard_meter_hemiola_rules SOURCES test_hemiola_rules.c LIBS ursatz_theory)

ursatz_test(standard_meter_syncopation_through_stronger SOURCES test_syncopation_through_stronger.c
    LIBS ursatz_theory)

ursatz_test(standard_meter_hypermeasure_rules SOURCES test_hypermeasure_rules.c LIBS ursatz_theory)

ursatz_test(standard_meter_accent_beats SOURCES test_accent_beats.c LIBS ursatz_theory VALGRIND)

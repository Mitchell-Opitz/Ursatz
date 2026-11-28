ursatz_test(knowledge_snapshot SOURCES test_knowledge_snapshot.c LIBS ursatz_theory VALGRIND)
ursatz_test(instrument_entries SOURCES test_instrument_entries.c LIBS ursatz_theory VALGRIND)
ursatz_test(vocabulary_entries SOURCES test_vocabulary_entries.c LIBS ursatz_theory cjson
    DEFINITIONS HINDEMITH_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../../../../knowledge/vocabulary/hindemith.json" VALGRIND)
ursatz_test(form_entries SOURCES test_form_entries.c LIBS ursatz_theory VALGRIND)
ursatz_test(rule_entries SOURCES test_rule_entries.c LIBS ursatz_theory VALGRIND)
ursatz_test(mode_entries SOURCES test_mode_entries.c LIBS ursatz_theory VALGRIND)
ursatz_test(pattern_entries SOURCES test_pattern_entries.c LIBS ursatz_theory VALGRIND)
ursatz_test(schema_entries SOURCES test_schema_entries.c LIBS ursatz_theory VALGRIND)

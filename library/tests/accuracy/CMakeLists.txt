ursatz_test_library(accuracy_support SOURCES romantext.c score_layout.c alignment.c LIBS ursatz_io
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR})

ursatz_test(accuracy_romantext SOURCES test_romantext.c LIBS accuracy_support LABELS accuracy)

ursatz_test(accuracy_score_layout SOURCES test_score_layout.c LIBS accuracy_support LABELS accuracy)

ursatz_test(accuracy_corpus_load SOURCES test_corpus_load.c LIBS accuracy_support ursatz_test_support
    DEFINITIONS URSATZ_ACCURACY_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/accuracy-corpus" LABELS accuracy)

# Every corpus chorale through the whole analysis chain to the form stage (P41 B6, R-396).
ursatz_test(accuracy_chorale_form SOURCES test_chorale_form.c LIBS accuracy_support ursatz_test_support ursatz_chain ursatz_analysis
    DEFINITIONS URSATZ_ACCURACY_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/accuracy-corpus" VALGRIND LABELS accuracy)

# The chain over the annotated corpus and the existing fixture tests, read through the harness. Each fixture
# wrapper compiles one tests/integration test as it is (main renamed) in its own library, linked to what that test
# links, because frameworks share header names (analysis_phrase.h is in more than one) and a translation unit must
# see only the include directories of its own frameworks. The libraries link into the harness link-only.
set(ACCURACY_FIXTURE_DEFINITIONS
    URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${URSATZ_FIXTURES_DIR}/annotations"
    URSATZ_FIXTURE_DIR="${URSATZ_FIXTURES_DIR}/galant"
    URSATZ_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")

# accuracy_fixture(name test_file libs...): the test file with its `int main(void)` renamed to accuracy_run_<name>,
# generated into the build directory (the test itself is not edited). Textual, because some tests include another
# test's .c for its helpers and undefine `main` while doing so, which a macro rename cannot survive.
function(accuracy_fixture name test_file)
  set(source ${PROJECT_SOURCE_DIR}/tests/integration/${test_file})
  set(generated ${CMAKE_CURRENT_BINARY_DIR}/fixtures/fixture_${name}.c)
  file(READ ${source} body)
  string(REPLACE "int main(void)" "int accuracy_run_${name}(void)" body "${body}")
  file(WRITE ${generated}.tmp "/* Generated from tests/integration/${test_file}: `int main(void)` renamed. */\n${body}")
  configure_file(${generated}.tmp ${generated} COPYONLY)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
  add_library(accuracy_fixture_${name} STATIC ${generated})
  target_include_directories(accuracy_fixture_${name} PRIVATE ${PROJECT_SOURCE_DIR}/tests/integration
                             ${PROJECT_SOURCE_DIR}/tests/analysis)
  target_compile_options(accuracy_fixture_${name} PRIVATE -w -UNDEBUG)
  # the tests that include the cadence test for its helpers rename its main; a distinct name per library keeps
  # those unused mains from colliding at link time
  target_compile_definitions(accuracy_fixture_${name} PRIVATE ${ACCURACY_FIXTURE_DEFINITIONS}
                             cadence_pieces_unused_main=accuracy_${name}_unused_cadence_main)
  target_link_libraries(accuracy_fixture_${name} PRIVATE ${ARGN})
endfunction()

accuracy_fixture(cadences test_cadences_reference_pieces.c ursatz_chain ursatz_analysis)
accuracy_fixture(form test_form_reference_pieces.c ursatz_analysis ursatz_chain)
accuracy_fixture(texture test_texture_field1.c ursatz_chain ursatz_analysis)
accuracy_fixture(rhythm test_rhythm_field5.c ursatz_analysis ursatz_chain)
accuracy_fixture(melody test_melody_bach114.c ursatz_chain ursatz_analysis)
accuracy_fixture(motive test_motive_mozart545.c ursatz_analysis ursatz_chain)
accuracy_fixture(phrase_model test_mozart545_phrase_model.c ursatz_chain ursatz_analysis)
accuracy_fixture(schema test_schema_sequence.c ursatz_chain ursatz_analysis)

# The chorale chain and the report: the analysis group (the cadences
# that give the key regions their second pass).
add_library(accuracy_chain STATIC chorale_chain.c)
target_include_directories(accuracy_chain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(accuracy_chain PUBLIC accuracy_support PRIVATE ursatz_chain ursatz_analysis)
target_compile_options(accuracy_chain PRIVATE -UNDEBUG)

ursatz_test_executable(accuracy_harness SOURCES accuracy_harness.c accuracy_report.c accuracy_gate.c fixtures.c
    LIBS accuracy_chain accuracy_fixture_cadences accuracy_fixture_form accuracy_fixture_texture accuracy_fixture_rhythm accuracy_fixture_melody accuracy_fixture_motive accuracy_fixture_phrase_model accuracy_fixture_schema DEFINITIONS URSATZ_ACCURACY_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/accuracy-corpus"
    URSATZ_ACCURACY_DIR="${CMAKE_CURRENT_SOURCE_DIR}" URSATZ_ACCURACY_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
# The harness fails when an overall metric is more than the baseline's tolerance below tests/accuracy/baseline.json.
ursatz_test(accuracy_harness TARGET accuracy_harness LABELS accuracy)

ursatz_test(accuracy_gate SOURCES test_accuracy_gate.c accuracy_gate.c accuracy_report.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} LABELS accuracy)

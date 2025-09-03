add_library(accuracy_support STATIC romantext.c score_layout.c alignment.c)
target_include_directories(accuracy_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(accuracy_support PUBLIC ursatz)

add_executable(test_accuracy_romantext test_romantext.c)
target_link_libraries(test_accuracy_romantext PRIVATE accuracy_support)
add_test(NAME accuracy_romantext COMMAND test_accuracy_romantext)

add_executable(test_accuracy_score_layout test_score_layout.c)
target_link_libraries(test_accuracy_score_layout PRIVATE accuracy_support)
add_test(NAME accuracy_score_layout COMMAND test_accuracy_score_layout)

add_executable(test_accuracy_corpus_load test_corpus_load.c)
target_link_libraries(test_accuracy_corpus_load PRIVATE accuracy_support)
target_compile_definitions(test_accuracy_corpus_load PRIVATE
    URSATZ_ACCURACY_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
add_test(NAME accuracy_corpus_load COMMAND test_accuracy_corpus_load)

# The chain over the annotated corpus and the existing fixture tests, read through the harness. Each fixture
# wrapper compiles one tests/integration test as it is (main renamed) in its own library, linked to what that test
# links, because frameworks share header names (analysis_phrase.h is in more than one) and a translation unit must
# see only the include directories of its own frameworks. The libraries link into the harness link-only.
set(ACCURACY_FIXTURE_DEFINITIONS
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_ANNOTATION_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/annotations"
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/galant"
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
  target_compile_options(accuracy_fixture_${name} PRIVATE -w)
  # the tests that include the cadence test for its helpers rename its main; a distinct name per library keeps
  # those unused mains from colliding at link time
  target_compile_definitions(accuracy_fixture_${name} PRIVATE ${ACCURACY_FIXTURE_DEFINITIONS}
                             cadence_pieces_unused_main=accuracy_${name}_unused_cadence_main)
  target_link_libraries(accuracy_fixture_${name} PRIVATE ${ARGN})
endfunction()

accuracy_fixture(cadences test_cadences_reference_pieces.c standard_harmony standard_tonality common_practice_minimal standard_meter)
accuracy_fixture(form test_form_reference_pieces.c standard_form standard_harmony standard_tonality common_practice_minimal standard_meter)
accuracy_fixture(texture test_texture_field1.c standard_texture standard_harmony standard_tonality common_practice_minimal standard_meter)
accuracy_fixture(rhythm test_rhythm_field5.c standard_form standard_harmony standard_tonality common_practice_minimal standard_meter)
accuracy_fixture(melody test_melody_bach114.c standard_counterpoint common_practice_minimal standard_meter)
accuracy_fixture(motive test_motive_mozart545.c standard_form standard_harmony standard_tonality common_practice_minimal standard_meter)
accuracy_fixture(phrase_model test_mozart545_phrase_model.c standard_harmony standard_tonality common_practice_minimal standard_meter)
accuracy_fixture(schema test_schema_sequence.c galant_schemas scale_harmonization standard_harmony standard_tonality common_practice_minimal
                 standard_meter)

# The chorale chain and the report: common-practice-minimal, standard-meter and standard-tonality only.
add_library(accuracy_chain STATIC chorale_chain.c)
target_include_directories(accuracy_chain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(accuracy_chain PUBLIC accuracy_support PRIVATE common_practice_minimal standard_meter standard_tonality)

add_executable(accuracy_harness accuracy_harness.c accuracy_report.c accuracy_gate.c fixtures.c)
target_link_libraries(accuracy_harness PRIVATE accuracy_chain
    accuracy_fixture_cadences accuracy_fixture_form accuracy_fixture_texture accuracy_fixture_rhythm
    accuracy_fixture_melody accuracy_fixture_motive accuracy_fixture_phrase_model accuracy_fixture_schema)
target_compile_definitions(accuracy_harness PRIVATE
    URSATZ_ACCURACY_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus"
    URSATZ_ACCURACY_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    URSATZ_ACCURACY_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
# The harness fails when an overall metric is more than the baseline's tolerance below tests/accuracy/baseline.json.
add_test(NAME accuracy_harness COMMAND accuracy_harness)

add_executable(test_accuracy_gate test_accuracy_gate.c accuracy_gate.c accuracy_report.c)
target_include_directories(test_accuracy_gate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME accuracy_gate COMMAND test_accuracy_gate WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

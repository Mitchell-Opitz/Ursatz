add_executable(test_analysis_pipeline test_analysis_pipeline.c)
target_link_libraries(test_analysis_pipeline PRIVATE ursatz_analyzer)
add_test(NAME test_analysis_pipeline COMMAND test_analysis_pipeline)

add_executable(test_report_output test_report_output.c)
target_link_libraries(test_report_output PRIVATE ursatz_analyzer)
add_test(NAME test_report_output COMMAND test_report_output)

add_executable(test_corpus_key_estimation test_corpus_key_estimation.c)
target_link_libraries(test_corpus_key_estimation PRIVATE ursatz_analyzer)
add_test(NAME test_corpus_key_estimation COMMAND test_corpus_key_estimation
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_long_labels test_long_labels.c)
target_link_libraries(test_long_labels PRIVATE ursatz_analyzer)
target_compile_definitions(test_long_labels PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_long_labels COMMAND test_long_labels)

add_executable(test_analysis_document test_analysis_document.c)
target_link_libraries(test_analysis_document PRIVATE ursatz_analyzer cjson)
add_test(NAME test_analysis_document COMMAND test_analysis_document)

add_test(NAME test_cli_output_flags
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DFIXTURE=${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_output_flags
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_output_flags.cmake)

# Optional: schema validation needs python3 with the jsonschema module.
find_package(Python3 COMPONENTS Interpreter QUIET)
set(URSATZ_ANALYZER_HAVE_JSONSCHEMA FALSE)
if(Python3_Interpreter_FOUND)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import jsonschema"
                    RESULT_VARIABLE jsonschema_result OUTPUT_QUIET ERROR_QUIET)
    if(jsonschema_result EQUAL 0)
        set(URSATZ_ANALYZER_HAVE_JSONSCHEMA TRUE)
    endif()
endif()
if(NOT URSATZ_ANALYZER_HAVE_JSONSCHEMA)
    message(STATUS "python3 with jsonschema not found: test_analysis_document_schema will be skipped")
endif()

add_test(NAME test_analysis_document_schema
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DPYTHON=${Python3_EXECUTABLE}
                 -DHAVE_JSONSCHEMA=${URSATZ_ANALYZER_HAVE_JSONSCHEMA}
                 -DVALIDATOR=${CMAKE_CURRENT_SOURCE_DIR}/validate_schema.py
                 -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/analysis-document.v5.schema.json
                 -DFIXTURE_DIR=${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures/framework-v1-corpus
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/analysis_document_schema
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_analysis_document.cmake)
set_tests_properties(test_analysis_document_schema PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED:")

add_executable(test_report_snapshots test_report_snapshots.c)
target_link_libraries(test_report_snapshots PRIVATE ursatz_analyzer)
target_compile_definitions(test_report_snapshots PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_report_snapshots COMMAND test_report_snapshots)

add_executable(test_measure_locations test_measure_locations.c)
target_link_libraries(test_measure_locations PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_measure_locations PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_measure_locations COMMAND test_measure_locations)

add_executable(test_chord_coverage test_chord_coverage.c)
target_link_libraries(test_chord_coverage PRIVATE ursatz_analyzer)
target_compile_definitions(test_chord_coverage PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
# Expected to fail: Field Nocturne No. 1 reads at 86.6% in local key regions (see README, Known limits). The 90%
# target stays; remove WILL_FAIL when the library reaches it.
add_test(NAME test_chord_coverage COMMAND test_chord_coverage)
set_tests_properties(test_chord_coverage PROPERTIES WILL_FAIL TRUE)

add_executable(test_chord_details test_chord_details.c)
target_link_libraries(test_chord_details PRIVATE ursatz_analyzer cjson)
add_test(NAME test_chord_details COMMAND test_chord_details)

add_executable(test_key_regions test_key_regions.c)
target_link_libraries(test_key_regions PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_key_regions PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
# Expected to fail: Ursatz's key-region detection does not reach the hand-read regions (see README, Known
# limits). The assertions stay as the target; remove WILL_FAIL when the library meets it.
add_test(NAME test_key_regions_clementi_36 COMMAND test_key_regions clementi)
add_test(NAME test_key_regions_mozart_545 COMMAND test_key_regions mozart)
set_tests_properties(test_key_regions_clementi_36 test_key_regions_mozart_545 PROPERTIES WILL_FAIL TRUE)
add_test(NAME test_key_regions_constructed COMMAND test_key_regions constructed)

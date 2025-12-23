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
                 -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/analysis-document.v9.schema.json
                 -DFIXTURE_DIR=${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures/framework-v1-corpus
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/analysis_document_schema
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_analysis_document.cmake)
set_tests_properties(test_analysis_document_schema PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED:")

add_executable(test_report_snapshots test_report_snapshots.c)
target_link_libraries(test_report_snapshots PRIVATE ursatz_analyzer)
target_compile_definitions(test_report_snapshots PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml"
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
add_test(NAME test_chord_coverage COMMAND test_chord_coverage)

add_executable(test_chord_details test_chord_details.c)
target_link_libraries(test_chord_details PRIVATE ursatz_analyzer cjson)
add_test(NAME test_chord_details COMMAND test_chord_details)

add_executable(test_key_regions test_key_regions.c)
target_link_libraries(test_key_regions PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_key_regions PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
# Expected to fail: Ursatz's key-region detection does not reach the hand-read regions (see README, Known
# limits). Measured with cadence-confirmed key changes (R-044): Clementi misses 1 core measure (m. 20), Mozart misses
# 38 core measures and 5 modulations: to G at m. 13, to C at m. 29, to G at m. 41 (no cadence arrives in them, so they
# are tonicizations), and the returns to C at m. 79 and m. 124. The assertions stay as the target; remove WILL_FAIL
# when the library meets it.
add_test(NAME test_key_regions_clementi_36 COMMAND test_key_regions clementi)
add_test(NAME test_key_regions_mozart_545 COMMAND test_key_regions mozart)
set_tests_properties(test_key_regions_clementi_36 test_key_regions_mozart_545 PROPERTIES WILL_FAIL TRUE)
add_test(NAME test_key_regions_constructed COMMAND test_key_regions constructed)

add_executable(test_roman_numerals test_roman_numerals.c)
target_link_libraries(test_roman_numerals PRIVATE ursatz_analyzer cjson m)
target_compile_definitions(test_roman_numerals PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_roman_numerals COMMAND test_roman_numerals)

add_executable(test_outer_voices test_outer_voices.c)
target_link_libraries(test_outer_voices PRIVATE ursatz_analyzer cjson)
add_test(NAME test_outer_voices COMMAND test_outer_voices)

add_executable(test_cadences test_cadences.c)
target_link_libraries(test_cadences PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_cadences PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_cadences COMMAND test_cadences)

add_executable(test_harmonic_function test_harmonic_function.c)
target_link_libraries(test_harmonic_function PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_harmonic_function PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_harmonic_function COMMAND test_harmonic_function)

add_executable(test_progressions test_progressions.c)
target_link_libraries(test_progressions PRIVATE ursatz_analyzer cjson)
target_include_directories(test_progressions PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(test_progressions PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_progressions COMMAND test_progressions)
# Expected to fail: the library's Roman numerals give Bach 114 the most common progression I > IV7 (6 occurrences,
# tied with ii > iii7), which ends on IV7, not I (see README, Known limits). The assertion stays as the target; remove
# WILL_FAIL when the library meets it.
add_test(NAME test_progressions_bach_114_top COMMAND test_progressions bach-top)
set_tests_properties(test_progressions_bach_114_top PROPERTIES WILL_FAIL TRUE)

add_executable(test_phrases test_phrases.c)
target_link_libraries(test_phrases PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_phrases PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_phrases COMMAND test_phrases)

add_executable(test_themes test_themes.c)
target_link_libraries(test_themes PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_themes PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_themes COMMAND test_themes)

add_executable(test_form_plan test_form_plan.c)
target_link_libraries(test_form_plan PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_form_plan PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_form_plan COMMAND test_form_plan)

add_executable(test_motives test_motives.c)
target_link_libraries(test_motives PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_motives PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_motives COMMAND test_motives)

add_executable(test_melody test_melody.c)
target_link_libraries(test_melody PRIVATE ursatz_analyzer cjson m)
target_compile_definitions(test_melody PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_melody COMMAND test_melody)
# Expected to fail: the extracted soprano of Bach 114 misses the annotated ranges (the extractor adopts a left-hand note
# where the right hand rests, at the start of measure 4) and gets the right recovery status for 4 of the 11 annotated
# leaps (36.4%, target 90%): it keeps one soprano note per segment, which drops the quick notes the other leaps arrive on.
# Both climaxes are right. The assertions stay as the target; remove WILL_FAIL when the extraction meets it.
add_test(NAME test_melody_bach_114_target COMMAND test_melody bach-114-target)
set_tests_properties(test_melody_bach_114_target PROPERTIES WILL_FAIL TRUE)

add_executable(test_rhythm test_rhythm.c)
target_link_libraries(test_rhythm PRIVATE ursatz_analyzer cjson m)
target_compile_definitions(test_rhythm PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_rhythm COMMAND test_rhythm)
add_test(NAME test_rhythm_constructed COMMAND test_rhythm constructed)
# Expected to fail: the form adapter reports no elided cadence, so the hypermeter has no reinterpretation, and Field 5's
# annotation has one (measure 9, where the phrase of measures 5 to 8 is elided with the next). The syncopation, the
# hemiolas (none) and 7 of 8 hyperdownbeats (87.5%, target 80%; measure 18 is missed) meet the target. The assertions
# stay as the target; remove WILL_FAIL when the form adapter reports elision.
add_test(NAME test_rhythm_field5_target COMMAND test_rhythm field5-target)
set_tests_properties(test_rhythm_field5_target PROPERTIES WILL_FAIL TRUE)

add_executable(test_texture test_texture.c)
target_link_libraries(test_texture PRIVATE ursatz_analyzer)
target_compile_definitions(test_texture PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures")
add_test(NAME test_texture COMMAND test_texture)

add_executable(test_voice_leading test_voice_leading.c)
target_link_libraries(test_voice_leading PRIVATE ursatz_analyzer)
target_compile_definitions(test_voice_leading PRIVATE
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_voice_leading COMMAND test_voice_leading)

add_executable(test_schemas test_schemas.c)
target_link_libraries(test_schemas PRIVATE ursatz_analyzer m)
target_compile_definitions(test_schemas PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures"
    URSATZ_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME test_schemas COMMAND test_schemas)

add_executable(test_musicxml_midi_agreement test_musicxml_midi_agreement.c)
target_link_libraries(test_musicxml_midi_agreement PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_musicxml_midi_agreement PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_musicxml_midi_agreement COMMAND test_musicxml_midi_agreement)

add_executable(test_musicxml_input test_musicxml_input.c)
target_link_libraries(test_musicxml_input PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_musicxml_input PRIVATE
    URSATZ_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_musicxml_input COMMAND test_musicxml_input)

add_test(NAME test_cli_input_formats
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DMIDI_FIXTURE=${PROJECT_SOURCE_DIR}/external/ursatz/tests/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid
                 -DMUSICXML_DIR=${CMAKE_CURRENT_SOURCE_DIR}/musicxml
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_input_formats
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_input_formats.cmake)

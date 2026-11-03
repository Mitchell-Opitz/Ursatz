add_library(analyzer_test_support STATIC support/test_support.c)
target_include_directories(analyzer_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(analyzer_test_support PUBLIC ursatz_analyzer cjson)

add_executable(test_analysis_pipeline test_analysis_pipeline.c)
target_link_libraries(test_analysis_pipeline PRIVATE ursatz_analyzer analyzer_test_support)
add_test(NAME test_analysis_pipeline COMMAND test_analysis_pipeline)

add_executable(test_report_output test_report_output.c)
target_link_libraries(test_report_output PRIVATE ursatz_analyzer analyzer_test_support)
add_test(NAME test_report_output COMMAND test_report_output)

add_executable(test_corpus_key_estimation test_corpus_key_estimation.c)
target_link_libraries(test_corpus_key_estimation PRIVATE ursatz_analyzer)
target_compile_definitions(test_corpus_key_estimation PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_corpus_key_estimation COMMAND test_corpus_key_estimation)

add_executable(test_long_labels test_long_labels.c)
target_link_libraries(test_long_labels PRIVATE ursatz_analyzer)
target_compile_definitions(test_long_labels PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_long_labels COMMAND test_long_labels)

add_executable(test_analysis_document test_analysis_document.c)
target_link_libraries(test_analysis_document PRIVATE ursatz_analyzer analyzer_test_support cjson)
add_test(NAME test_analysis_document COMMAND test_analysis_document)

add_test(NAME test_cli_output_flags
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid
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
                 -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/framework-v1-corpus
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/analysis_document_schema
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_analysis_document.cmake)
set_tests_properties(test_analysis_document_schema PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED:")

add_executable(test_report_snapshots test_report_snapshots.c)
target_link_libraries(test_report_snapshots PRIVATE ursatz_analyzer analyzer_test_support)
target_compile_definitions(test_report_snapshots PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_report_snapshots COMMAND test_report_snapshots)

add_executable(test_document_snapshots test_document_snapshots.c)
target_link_libraries(test_document_snapshots PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_document_snapshots PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_document_snapshots COMMAND test_document_snapshots)

add_executable(test_analysis_result test_analysis_result.c)
target_link_libraries(test_analysis_result PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_analysis_result PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_analysis_result_smoke COMMAND test_analysis_result smoke)
add_test(NAME test_analysis_result COMMAND test_analysis_result)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_test(NAME test_analysis_result_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_analysis_result> smoke)
    set_tests_properties(test_analysis_result_valgrind PROPERTIES LABELS slow)
endif()

add_executable(test_chord_sequence test_chord_sequence.c)
target_link_libraries(test_chord_sequence PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_chord_sequence PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_chord_sequence_smoke COMMAND test_chord_sequence smoke)
add_test(NAME test_chord_sequence COMMAND test_chord_sequence)
set_tests_properties(test_chord_sequence PROPERTIES LABELS slow)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_test(NAME test_chord_sequence_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_chord_sequence> smoke)
    set_tests_properties(test_chord_sequence_valgrind PROPERTIES LABELS slow)
endif()

add_executable(test_playback test_playback.c)
target_link_libraries(test_playback PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_playback PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml"
    URSATZ_ANALYZER_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_playback COMMAND test_playback)
add_test(NAME test_playback_smoke COMMAND test_playback smoke)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_test(NAME test_playback_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_playback> smoke)
    set_tests_properties(test_playback_valgrind PROPERTIES LABELS slow)
endif()

add_executable(test_score_export test_score_export.c)
target_link_libraries(test_score_export PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_score_export PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml"
    URSATZ_ANALYZER_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_score_export COMMAND test_score_export)
add_test(NAME test_score_export_smoke COMMAND test_score_export smoke)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_test(NAME test_score_export_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_score_export> smoke)
    set_tests_properties(test_score_export_valgrind PROPERTIES LABELS slow)
endif()

add_executable(test_measure_locations test_measure_locations.c)
target_link_libraries(test_measure_locations PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_measure_locations PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_measure_locations COMMAND test_measure_locations)

add_executable(test_chord_coverage test_chord_coverage.c)
target_link_libraries(test_chord_coverage PRIVATE ursatz_analyzer)
target_compile_definitions(test_chord_coverage PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_chord_coverage COMMAND test_chord_coverage)

add_executable(test_chord_details test_chord_details.c)
target_link_libraries(test_chord_details PRIVATE ursatz_analyzer analyzer_test_support cjson)
add_test(NAME test_chord_details COMMAND test_chord_details)

add_executable(test_key_regions test_key_regions.c)
target_link_libraries(test_key_regions PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_key_regions PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
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
target_link_libraries(test_roman_numerals PRIVATE ursatz_analyzer analyzer_test_support cjson m)
target_compile_definitions(test_roman_numerals PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_roman_numerals COMMAND test_roman_numerals)

add_executable(test_outer_voices test_outer_voices.c)
target_link_libraries(test_outer_voices PRIVATE ursatz_analyzer cjson)
add_test(NAME test_outer_voices COMMAND test_outer_voices)

add_executable(test_cadences test_cadences.c)
target_link_libraries(test_cadences PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_cadences PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_cadences COMMAND test_cadences)

add_executable(test_harmonic_function test_harmonic_function.c)
target_link_libraries(test_harmonic_function PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_harmonic_function PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_harmonic_function COMMAND test_harmonic_function)

add_executable(test_progressions test_progressions.c)
target_link_libraries(test_progressions PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_include_directories(test_progressions PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(test_progressions PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_progressions COMMAND test_progressions)
# Expected to fail: the library's Roman numerals give Bach 114 the most common progression I > IV7 (6 occurrences,
# tied with ii > iii7), which ends on IV7, not I (see README, Known limits). The assertion stays as the target; remove
# WILL_FAIL when the library meets it.
add_test(NAME test_progressions_bach_114_top COMMAND test_progressions bach-top)
set_tests_properties(test_progressions_bach_114_top PROPERTIES WILL_FAIL TRUE)

add_executable(test_phrases test_phrases.c)
target_link_libraries(test_phrases PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_phrases PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_phrases COMMAND test_phrases)

add_executable(test_themes test_themes.c)
target_link_libraries(test_themes PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_themes PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_themes COMMAND test_themes)

add_executable(test_form_plan test_form_plan.c)
target_link_libraries(test_form_plan PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_form_plan PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_form_plan COMMAND test_form_plan)

add_executable(test_motives test_motives.c)
target_link_libraries(test_motives PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_motives PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_motives COMMAND test_motives)

add_executable(test_motive_sketch_map test_motive_sketch_map.c)
target_link_libraries(test_motive_sketch_map PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_include_directories(test_motive_sketch_map PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(test_motive_sketch_map PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_motive_sketch_map COMMAND test_motive_sketch_map)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_test(NAME test_motive_sketch_map_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_motive_sketch_map>)
    set_tests_properties(test_motive_sketch_map_valgrind PROPERTIES LABELS slow)
endif()

add_executable(test_melody test_melody.c)
target_link_libraries(test_melody PRIVATE ursatz_analyzer analyzer_test_support cjson m)
target_compile_definitions(test_melody PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_melody COMMAND test_melody)
# Expected to fail: the extracted soprano of Bach 114 misses the annotated ranges (the extractor adopts a left-hand note
# where the right hand rests, at the start of measure 4) and gets the right recovery status for 4 of the 11 annotated
# leaps (36.4%, target 90%): it keeps one soprano note per segment, which drops the quick notes the other leaps arrive on.
# Both climaxes are right. The assertions stay as the target; remove WILL_FAIL when the extraction meets it.
add_test(NAME test_melody_bach_114_target COMMAND test_melody bach-114-target)
set_tests_properties(test_melody_bach_114_target PROPERTIES WILL_FAIL TRUE)

add_executable(test_rhythm test_rhythm.c)
target_link_libraries(test_rhythm PRIVATE ursatz_analyzer analyzer_test_support cjson m)
target_compile_definitions(test_rhythm PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
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
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_texture COMMAND test_texture)

add_executable(test_voice_leading test_voice_leading.c)
target_link_libraries(test_voice_leading PRIVATE ursatz_analyzer)
target_compile_definitions(test_voice_leading PRIVATE
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_voice_leading COMMAND test_voice_leading)

add_executable(test_markings test_markings.c)
target_link_libraries(test_markings PRIVATE ursatz_analyzer)
target_compile_definitions(test_markings PRIVATE
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_markings COMMAND test_markings)

add_executable(test_schemas test_schemas.c)
target_link_libraries(test_schemas PRIVATE ursatz_analyzer m)
target_compile_definitions(test_schemas PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME test_schemas COMMAND test_schemas)

add_executable(test_musicxml_midi_agreement test_musicxml_midi_agreement.c)
target_link_libraries(test_musicxml_midi_agreement PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_musicxml_midi_agreement PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_musicxml_midi_agreement COMMAND test_musicxml_midi_agreement)

add_executable(test_musicxml_input test_musicxml_input.c)
target_link_libraries(test_musicxml_input PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_musicxml_input PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_musicxml_input COMMAND test_musicxml_input)

add_executable(test_midi_parts test_midi_parts.c)
target_link_libraries(test_midi_parts PRIVATE ursatz_analyzer)
add_test(NAME test_midi_parts COMMAND test_midi_parts)
find_program(VALGRIND_PROGRAM valgrind)

add_executable(test_tempo_map_import test_tempo_map_import.c)
target_link_libraries(test_tempo_map_import PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_include_directories(test_tempo_map_import PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_tempo_map_import PRIVATE URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_tempo_map_import COMMAND test_tempo_map_import)
if(VALGRIND_PROGRAM)
    add_test(NAME test_tempo_map_import_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_tempo_map_import>)
endif()

add_executable(test_notation_reading test_notation_reading.c)
target_link_libraries(test_notation_reading PRIVATE ursatz_analyzer)
target_compile_definitions(test_notation_reading PRIVATE URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_notation_reading COMMAND test_notation_reading)

add_test(NAME test_cli_notation_reading
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/musicxml/bach-114.musicxml
                 -DPYTHON=${Python3_EXECUTABLE}
                 -DHAVE_JSONSCHEMA=${URSATZ_ANALYZER_HAVE_JSONSCHEMA}
                 -DVALIDATOR=${CMAKE_CURRENT_SOURCE_DIR}/validate_schema.py
                 -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/analysis-document.v9.schema.json
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_notation_reading
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_notation_reading.cmake)

add_test(NAME test_cli_input_formats
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DMIDI_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid
                 -DMUSICXML_DIR=${CMAKE_CURRENT_SOURCE_DIR}/musicxml
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_input_formats
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_input_formats.cmake)

add_executable(test_counterpoint_check test_counterpoint_check.c)
target_link_libraries(test_counterpoint_check PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_include_directories(test_counterpoint_check PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(test_counterpoint_check PRIVATE
    URSATZ_COUNTERPOINT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/counterpoint"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_counterpoint_check COMMAND test_counterpoint_check)

# The CLI's exit code: 0 for a two-voice input, 2 (with the rest of the analysis written) for one that is not two voices,
# and a usage error (1) for a bad species or --cantus without --check-counterpoint.
add_test(NAME test_cli_counterpoint_check
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/counterpoint
                 -DNOT_TWO_VOICES=${CMAKE_CURRENT_SOURCE_DIR}/musicxml/bach-114.musicxml
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_counterpoint_check
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_counterpoint_check.cmake)

# Test corpora, built in the build tree at configure time (never committed twice). "reference" is the five framework-v1
# pieces; "mixed" has one MIDI, one MusicXML in a subfolder, one .txt and one broken .mid.
set(URSATZ_CORPORA_DIR ${CMAKE_CURRENT_BINARY_DIR}/corpora)
set(URSATZ_REFERENCE_FIXTURES ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/framework-v1-corpus)
file(REMOVE_RECURSE ${URSATZ_CORPORA_DIR})
file(GLOB reference_pieces ${URSATZ_REFERENCE_FIXTURES}/*.mid)
file(COPY ${reference_pieces} DESTINATION ${URSATZ_CORPORA_DIR}/reference)
file(COPY "${URSATZ_REFERENCE_FIXTURES}/Bach 114.mid" DESTINATION ${URSATZ_CORPORA_DIR}/mixed)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/musicxml/field-1.musicxml DESTINATION ${URSATZ_CORPORA_DIR}/mixed/sub)
file(WRITE ${URSATZ_CORPORA_DIR}/mixed/notes.txt "not a piece\n")
file(WRITE ${URSATZ_CORPORA_DIR}/mixed/broken.mid "not a midi file\n")
# "chorales" is the ten accuracy-corpus chorales (private use, R-016), one MusicXML file each;
# "sonatinas" is the two sonatina movements.
file(GLOB chorale_scores ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/chorales/*.musicxml)
file(COPY ${chorale_scores} DESTINATION ${URSATZ_CORPORA_DIR}/chorales)
file(COPY "${URSATZ_REFERENCE_FIXTURES}/Clementi 36.mid" "${URSATZ_REFERENCE_FIXTURES}/Mozart 545.mid"
     DESTINATION ${URSATZ_CORPORA_DIR}/sonatinas)

add_executable(test_corpus test_corpus.c)
target_link_libraries(test_corpus PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_corpus PRIVATE
    URSATZ_CORPORA_DIR="${URSATZ_CORPORA_DIR}"
    URSATZ_CORPUS_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/corpus_work"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_corpus COMMAND test_corpus)

add_test(NAME test_cli_corpus
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                 -DFIXTURE=${URSATZ_REFERENCE_FIXTURES}/Field\ 1.mid
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_corpus
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_corpus.cmake)

add_test(NAME test_corpus_index_schema
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DPYTHON=${Python3_EXECUTABLE}
                 -DHAVE_JSONSCHEMA=${URSATZ_ANALYZER_HAVE_JSONSCHEMA}
                 -DVALIDATOR=${CMAKE_CURRENT_SOURCE_DIR}/validate_schema.py
                 -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/corpus-index.v1.schema.json
                 -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/corpus_index_schema
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_corpus_index.cmake)
set_tests_properties(test_corpus_index_schema PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED:")

add_executable(test_style_labels test_style_labels.c)
target_link_libraries(test_style_labels PRIVATE ursatz_analyzer)
add_test(NAME test_style_labels COMMAND test_style_labels)

add_executable(test_style_profile test_style_profile.c)
target_link_libraries(test_style_profile PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_style_profile PRIVATE
    URSATZ_CORPORA_DIR="${URSATZ_CORPORA_DIR}"
    URSATZ_STYLE_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/style_work"
    PROJECT_VERSION_FOR_TESTS="${PROJECT_VERSION}"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_style_profile COMMAND test_style_profile)

add_test(NAME test_cli_style_profile
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_style_profile
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_style_profile.cmake)

add_test(NAME test_style_model_schema
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DPYTHON=${Python3_EXECUTABLE}
                 -DHAVE_JSONSCHEMA=${URSATZ_ANALYZER_HAVE_JSONSCHEMA}
                 -DVALIDATOR=${CMAKE_CURRENT_SOURCE_DIR}/validate_schema.py
                 -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/style-model.v1.schema.json
                 -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/style_model_schema
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_style_model.cmake)
set_tests_properties(test_style_model_schema PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED:")

add_executable(test_similar_material test_similar_material.c)
target_link_libraries(test_similar_material PRIVATE ursatz_analyzer analyzer_test_support cjson)
target_compile_definitions(test_similar_material PRIVATE
    URSATZ_CORPORA_DIR="${URSATZ_CORPORA_DIR}"
    URSATZ_SIMILAR_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/similar_work"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_similar_material COMMAND test_similar_material)

# The --similar flag: the motive file comes from the test program, which writes it.
add_test(NAME test_cli_similar_motive_file COMMAND test_similar_material --write-motive ${CMAKE_CURRENT_BINARY_DIR}/cli_similar_motive.mid)
set_tests_properties(test_cli_similar_motive_file PROPERTIES FIXTURES_SETUP similar_motive_file)
add_test(NAME test_cli_similar
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                 -DMOTIVE=${CMAKE_CURRENT_BINARY_DIR}/cli_similar_motive.mid
                 -DPYTHON=${Python3_EXECUTABLE}
                 -DHAVE_JSONSCHEMA=${URSATZ_ANALYZER_HAVE_JSONSCHEMA}
                 -DVALIDATOR=${CMAKE_CURRENT_SOURCE_DIR}/validate_schema.py
                 -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/similar-material.v1.schema.json
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_similar
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_similar.cmake)
set_tests_properties(test_cli_similar PROPERTIES FIXTURES_REQUIRED similar_motive_file)

add_executable(test_composer_api test_composer_api.c)
target_link_libraries(test_composer_api PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_composer_api PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_CORPORA_DIR="${URSATZ_CORPORA_DIR}"
    URSATZ_COMPOSER_API_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/composer_api_work")
add_test(NAME test_composer_api_smoke COMMAND test_composer_api smoke)
add_test(NAME test_composer_api COMMAND test_composer_api)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_test(NAME test_composer_api_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_composer_api> smoke)
    set_tests_properties(test_composer_api_valgrind PROPERTIES LABELS slow)
endif()

add_executable(test_hearing test_hearing.c)
target_link_libraries(test_hearing PRIVATE ursatz_analyzer cjson)
target_include_directories(test_hearing PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(test_hearing PRIVATE
    URSATZ_ANALYZER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    URSATZ_HEARING_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/hearing_work")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/hearing_work)
add_test(NAME test_hearing_smoke COMMAND test_hearing smoke)
add_test(NAME test_hearing COMMAND test_hearing)
set_tests_properties(test_hearing PROPERTIES LABELS slow)
if(VALGRIND_PROGRAM)
    add_test(NAME test_hearing_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_hearing> smoke)
    set_tests_properties(test_hearing_valgrind PROPERTIES LABELS slow)
endif()

add_executable(test_meter_check test_meter_check.c)
target_link_libraries(test_meter_check PRIVATE ursatz_analyzer analyzer_test_support cjson m)
target_compile_definitions(test_meter_check PRIVATE URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
add_test(NAME test_meter_check COMMAND test_meter_check)

add_executable(test_motive_links test_motive_links.c)
target_link_libraries(test_motive_links PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_motive_links PRIVATE
    URSATZ_LINKS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/links"
    URSATZ_LINKS_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/motive_links_work")
add_test(NAME test_motive_links COMMAND test_motive_links)
if(VALGRIND_PROGRAM)
    add_test(NAME test_motive_links_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_motive_links>)
    set_tests_properties(test_motive_links_valgrind PROPERTIES LABELS slow)
endif()
add_test(NAME test_cli_link_motives
         COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz-analyzer>
                 -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/links
                 -DPYTHON=${Python3_EXECUTABLE}
                 -DHAVE_JSONSCHEMA=${URSATZ_ANALYZER_HAVE_JSONSCHEMA}
                 -DVALIDATOR=${CMAKE_CURRENT_SOURCE_DIR}/validate_schema.py
                 -DLINKS_SCHEMA=${PROJECT_SOURCE_DIR}/schema/motive-links.v1.schema.json
                 -DCACHE_SCHEMA=${PROJECT_SOURCE_DIR}/schema/motive-link-cache.v1.schema.json
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_link_motives
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_link_motives.cmake)

add_executable(test_staleness test_staleness.c)
target_link_libraries(test_staleness PRIVATE ursatz_analyzer cjson)
target_compile_definitions(test_staleness PRIVATE
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
add_test(NAME test_staleness COMMAND test_staleness)
if(VALGRIND_PROGRAM)
    add_test(NAME test_staleness_valgrind
             COMMAND ${VALGRIND_PROGRAM} --error-exitcode=1 --leak-check=full --errors-for-leak-kinds=definite,indirect
                     --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp
                     $<TARGET_FILE:test_staleness>)
endif()

add_executable(test_version test_version.c)
target_link_libraries(test_version PRIVATE ursatz_analyzer)
add_test(NAME test_version COMMAND test_version)

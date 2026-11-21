# Every test may include the snapshot helper (support/test_knowledge.h).
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/support)

ursatz_test_library(analyzer_test_support SOURCES support/test_support.c LIBS ursatz_analyzer_engine ursatz_test_support cjson
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/support ${PROJECT_SOURCE_DIR}/src)

ursatz_test(test_analysis_pipeline LIBS ursatz_analyzer_engine analyzer_test_support)

# Over 10 s in the fast set (about 38 s of CPU measured): runs nightly.
ursatz_test(test_corpus_key_estimation LIBS ursatz_analyzer_engine analyzer_test_support
    LABELS slow)

# Over 10 s in the fast set (about 37 s of CPU measured): runs nightly.
ursatz_test(test_long_labels LIBS ursatz_analyzer_engine analyzer_test_support
    LABELS slow)

ursatz_test(test_analysis_document LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_ANALYSIS_SCHEMA="${PROJECT_SOURCE_DIR}/schema/analysis-document.schema.json")

ursatz_test(test_cli_output_flags COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz>
                 -DFIXTURE=${URSATZ_FIXTURES_DIR}/arpeggiation-investigation/Bach_Prelude_C_Major.mid
                 "-DBACH_114=${URSATZ_FIXTURES_DIR}/framework-v1-corpus/Bach 114.mid"
                 -DEXPORT_GOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/snapshots/bach-114.export.txt
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_output_flags
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_output_flags.cmake)

if(URSATZ_JSONSCHEMA_PYTHON)
    # The smoke validates the Bach 114 document only.
    ursatz_test(test_analysis_document_schema_smoke COMMAND ${CMAKE_COMMAND}
                     -DCLI=$<TARGET_FILE:ursatz>
                     -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
                     -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                     -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/analysis-document.schema.json
                     -DFIXTURE_DIR=${URSATZ_FIXTURES_DIR}/framework-v1-corpus
                     "-DONLY=Bach 114.mid"
                     -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/analysis_document_schema_smoke
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_analysis_document.cmake
        LABELS schema)
    # Over 10 s in the fast set (about 49 s of CPU measured): runs nightly.
    ursatz_test(test_analysis_document_schema COMMAND ${CMAKE_COMMAND}
                     -DCLI=$<TARGET_FILE:ursatz>
                     -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
                     -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                     -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/analysis-document.schema.json
                     -DFIXTURE_DIR=${URSATZ_FIXTURES_DIR}/framework-v1-corpus
                     -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/analysis_document_schema
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_analysis_document.cmake
        LABELS schema slow)
endif()

# Over 10 s in the fast set (about 39 s of CPU measured): runs nightly.
ursatz_test(test_document_snapshots LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots" LABELS golden slow)

# The text of the document of Bach 114, byte for byte (R-631; a few seconds, in the fast set).
ursatz_test_executable(test_document_bytes LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
ursatz_test(test_document_bytes_smoke TARGET test_document_bytes LABELS golden)

ursatz_test_executable(test_analysis_result LIBS ursatz_analyzer_engine analyzer_test_support cjson ursatz_chain
    INCLUDES ${PROJECT_SOURCE_DIR}/src
    DEFINITIONS URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
ursatz_test(test_analysis_result_smoke TARGET test_analysis_result ARGS smoke)
# Over 10 s in the fast set (about 43 s of CPU measured): runs nightly.
ursatz_test(test_analysis_result TARGET test_analysis_result LABELS slow)
if(URSATZ_VALGRIND)
    ursatz_test(test_analysis_result_valgrind COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS}
        --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp $<TARGET_FILE:test_analysis_result> smoke LABELS slow
        valgrind)
endif()

ursatz_test_executable(test_chord_sequence LIBS ursatz_analyzer_engine analyzer_test_support cjson ursatz_chain)
ursatz_test(test_chord_sequence_smoke TARGET test_chord_sequence ARGS smoke)
ursatz_test(test_chord_sequence TARGET test_chord_sequence LABELS slow)
if(URSATZ_VALGRIND)
    ursatz_test(test_chord_sequence_valgrind COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS}
        --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp $<TARGET_FILE:test_chord_sequence> smoke LABELS slow
        valgrind)
endif()

ursatz_test_executable(test_playback LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
ursatz_test(test_playback TARGET test_playback LABELS golden)
ursatz_test(test_playback_smoke TARGET test_playback ARGS smoke LABELS golden)
if(URSATZ_VALGRIND)
    ursatz_test(test_playback_valgrind COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS}
        --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp $<TARGET_FILE:test_playback> smoke LABELS slow valgrind)
endif()

ursatz_test_executable(test_score_export LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml" URSATZ_ANALYZER_TMP_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
ursatz_test(test_score_export TARGET test_score_export LABELS golden PROPERTIES RESOURCE_LOCK analyzer_score_export_files)
ursatz_test(test_score_export_smoke TARGET test_score_export ARGS smoke LABELS golden
    PROPERTIES RESOURCE_LOCK analyzer_score_export_files)
if(URSATZ_VALGRIND)
    ursatz_test(test_score_export_valgrind COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS}
        --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp $<TARGET_FILE:test_score_export> smoke LABELS slow
        valgrind PROPERTIES RESOURCE_LOCK analyzer_score_export_files)
endif()

ursatz_test_executable(test_measure_locations LIBS ursatz_analyzer_engine analyzer_test_support cjson)
ursatz_test(test_measure_locations_smoke TARGET test_measure_locations ARGS smoke)
# Over 10 s in the fast set (about 40 s of CPU measured): runs nightly.
ursatz_test(test_measure_locations TARGET test_measure_locations LABELS slow)

ursatz_test_executable(test_chord_coverage LIBS ursatz_analyzer_engine analyzer_test_support)
ursatz_test(test_chord_coverage_smoke TARGET test_chord_coverage ARGS smoke)
# Over 10 s in the fast set (about 38 s of CPU measured): runs nightly.
ursatz_test(test_chord_coverage TARGET test_chord_coverage LABELS slow)

ursatz_test(test_chord_details LIBS ursatz_analyzer_engine analyzer_test_support cjson)

ursatz_test_executable(test_key_regions LIBS ursatz_analyzer_engine analyzer_test_support cjson)
# Both pass since keys are placed on phrases (Ursatz P42, R-397): the regions follow the phrases that arrive in a
# new key. Before P42 they failed (Clementi missed 1 core measure, Mozart 38 core measures and 5 modulations).
ursatz_test(test_key_regions_clementi_36 TARGET test_key_regions ARGS clementi)
# Over 10 s in the fast set (about 39 s of CPU measured): runs nightly.
ursatz_test(test_key_regions_mozart_545 TARGET test_key_regions ARGS mozart LABELS slow)
ursatz_test(test_key_regions_constructed TARGET test_key_regions ARGS constructed)

ursatz_test_executable(test_roman_numerals LIBS ursatz_analyzer_engine analyzer_test_support cjson m)
ursatz_test(test_roman_numerals_smoke TARGET test_roman_numerals ARGS smoke)
# Over 10 s in the fast set (about 42 s of CPU measured): runs nightly.
ursatz_test(test_roman_numerals TARGET test_roman_numerals LABELS slow)

ursatz_test(test_outer_voices LIBS ursatz_analyzer_engine analyzer_test_support cjson)

ursatz_test_executable(test_cadences LIBS ursatz_analyzer_engine analyzer_test_support cjson)
ursatz_test(test_cadences_smoke TARGET test_cadences ARGS smoke)
# Over 10 s in the fast set (about 39 s of CPU measured): runs nightly.
ursatz_test(test_cadences TARGET test_cadences LABELS slow)

ursatz_test_executable(test_harmonic_function LIBS ursatz_analyzer_engine analyzer_test_support cjson)
ursatz_test(test_harmonic_function_smoke TARGET test_harmonic_function ARGS smoke)
# Over 10 s in the fast set (about 40 s of CPU measured): runs nightly.
ursatz_test(test_harmonic_function TARGET test_harmonic_function LABELS slow)

ursatz_test_executable(test_progressions LIBS ursatz_analyzer_engine analyzer_test_support cjson
    INCLUDES ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/../library)
ursatz_test(test_progressions_smoke TARGET test_progressions ARGS smoke)
# Over 10 s in the fast set (about 38 s of CPU measured): runs nightly.
ursatz_test(test_progressions TARGET test_progressions LABELS slow)
# The top progression of Bach 114 is I > IV > vi (4 occurrences), tied with ii > I; the first progression that ends on I
# is the second (index 1). The test asserts those numbers and prints the target, the most common progression ends on I
# (see README, Known limits; R-698).
ursatz_test(test_progressions_bach_114_top TARGET test_progressions ARGS bach-top)

ursatz_test_executable(test_phrases LIBS ursatz_analyzer_engine analyzer_test_support cjson)
ursatz_test(test_phrases_smoke TARGET test_phrases ARGS smoke)
# Over 10 s in the fast set (about 42 s of CPU measured): runs nightly.
ursatz_test(test_phrases TARGET test_phrases LABELS slow)

ursatz_test_executable(test_themes LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
ursatz_test(test_themes_smoke TARGET test_themes ARGS smoke)
# Over 10 s in the fast set (about 44 s of CPU measured): runs nightly.
ursatz_test(test_themes TARGET test_themes LABELS slow)

ursatz_test_executable(test_form_plan LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
ursatz_test(test_form_plan_smoke TARGET test_form_plan ARGS smoke)
# Over 10 s in the fast set (about 46 s of CPU measured): runs nightly.
ursatz_test(test_form_plan TARGET test_form_plan LABELS slow)

# Over 10 s in the fast set (about 80 s of CPU measured): runs nightly.
ursatz_test(test_motives LIBS ursatz_analyzer_engine analyzer_test_support cjson
    LABELS slow)

# Over 10 s in the fast set (about 37 s of CPU measured): runs nightly.
ursatz_test(test_motive_sketch_map LIBS ursatz_analyzer_engine analyzer_test_support cjson INCLUDES ${PROJECT_SOURCE_DIR}/src
    LABELS slow)
if(URSATZ_VALGRIND)
    ursatz_test(test_motive_sketch_map_valgrind COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS}
        --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp $<TARGET_FILE:test_motive_sketch_map> LABELS slow
        valgrind)
endif()

ursatz_test_executable(test_melody LIBS ursatz_analyzer_engine analyzer_test_support cjson m)
# The extracted soprano of Bach 114 gets both climaxes, no range (the extractor adopts a left-hand note where the right
# hand rests) and 4 of the 11 annotated leaps (36.4%): the test asserts those floors and prints the target, both ranges
# and 90% of the leaps (R-698).
ursatz_test(test_melody TARGET test_melody)

ursatz_test_executable(test_rhythm LIBS ursatz_analyzer_engine analyzer_test_support cjson m)
# Field 5: the syncopation and the hemiolas are right, 5 of the 8 annotated hyperdownbeats are found (measures 18, 20 and
# 22 are missed), and the form adapter reports no elided cadence, so no reinterpretation: the test asserts those floors and
# prints the target, 80% of the hyperdownbeats and every reinterpretation (R-698).
ursatz_test(test_rhythm TARGET test_rhythm)
ursatz_test(test_rhythm_constructed TARGET test_rhythm ARGS constructed)

ursatz_test(test_analyzer_texture SOURCES test_texture.c LIBS ursatz_test_support ursatz_analyzer_engine analyzer_test_support)

ursatz_test(test_voice_leading LIBS ursatz_test_support ursatz_analyzer_engine analyzer_test_support
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")

ursatz_test(test_markings LIBS ursatz_test_support ursatz_analyzer_engine analyzer_test_support
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")

ursatz_test_executable(test_schemas LIBS ursatz_test_support ursatz_analyzer_engine analyzer_test_support m
    DEFINITIONS URSATZ_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
ursatz_test(test_schemas_smoke TARGET test_schemas ARGS smoke)
# Over 10 s in the fast set (about 39 s of CPU measured): runs nightly.
ursatz_test(test_schemas TARGET test_schemas LABELS slow)

ursatz_test_executable(test_musicxml_midi_agreement LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
ursatz_test(test_musicxml_midi_agreement_smoke TARGET test_musicxml_midi_agreement ARGS smoke)
# Over 10 s in the fast set (about 76 s of CPU measured): runs nightly.
ursatz_test(test_musicxml_midi_agreement TARGET test_musicxml_midi_agreement LABELS slow)

ursatz_test(test_musicxml_input LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")

ursatz_test(test_midi_parts LIBS ursatz_analyzer_engine analyzer_test_support)

ursatz_test(test_tempo_map_import LIBS ursatz_analyzer_engine analyzer_test_support cjson
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR} DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml"
    PROPERTIES RESOURCE_LOCK analyzer_tempo_map_file)
if(URSATZ_VALGRIND)
    ursatz_test(test_tempo_map_import_valgrind COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS}
        --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp $<TARGET_FILE:test_tempo_map_import> LABELS slow valgrind
        PROPERTIES RESOURCE_LOCK analyzer_tempo_map_file)
endif()

ursatz_test(test_notation_reading LIBS ursatz_analyzer_engine
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")

ursatz_test(test_cli_notation_reading COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz>
                 -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/musicxml/bach-114.musicxml
                 -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
                 -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                 -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/analysis-document.schema.json
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_notation_reading
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_notation_reading.cmake)

ursatz_test(test_cli_input_formats COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz>
                 -DMIDI_FIXTURE=${URSATZ_FIXTURES_DIR}/arpeggiation-investigation/Bach_Prelude_C_Major.mid
                 -DMUSICXML_DIR=${CMAKE_CURRENT_SOURCE_DIR}/musicxml
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_input_formats
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_input_formats.cmake)

# Test corpora, built in the build tree at configure time (never committed twice). "reference" is the five framework-v1
# pieces; "mixed" has one MIDI, one MusicXML in a subfolder, one .txt and one broken .mid.
set(URSATZ_CORPORA_DIR ${CMAKE_CURRENT_BINARY_DIR}/corpora)
set(URSATZ_REFERENCE_FIXTURES ${URSATZ_FIXTURES_DIR}/framework-v1-corpus)
file(REMOVE_RECURSE ${URSATZ_CORPORA_DIR})
file(GLOB reference_pieces ${URSATZ_REFERENCE_FIXTURES}/*.mid)
file(COPY ${reference_pieces} DESTINATION ${URSATZ_CORPORA_DIR}/reference)
file(COPY "${URSATZ_REFERENCE_FIXTURES}/Bach 114.mid" DESTINATION ${URSATZ_CORPORA_DIR}/mixed)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/musicxml/field-1.musicxml DESTINATION ${URSATZ_CORPORA_DIR}/mixed/sub)
file(WRITE ${URSATZ_CORPORA_DIR}/mixed/notes.txt "not a piece\n")
file(WRITE ${URSATZ_CORPORA_DIR}/mixed/broken.mid "not a midi file\n")
# "chorales" is the ten accuracy-corpus chorales (private use, R-016), one MusicXML file each;
# "sonatinas" is the two sonatina movements; "small" is Bach 114 and Clementi 36 (the smoke runs of the CLI tests).
file(MAKE_DIRECTORY ${URSATZ_CORPORA_DIR}/chorales)
file(GLOB chorale_folders LIST_DIRECTORIES true ${URSATZ_FIXTURES_DIR}/accuracy-corpus/chorale-*)
foreach(chorale_folder IN LISTS chorale_folders)
    get_filename_component(chorale ${chorale_folder} NAME)
    file(COPY_FILE ${chorale_folder}/score.musicxml ${URSATZ_CORPORA_DIR}/chorales/${chorale}.musicxml)
endforeach()
file(COPY "${URSATZ_REFERENCE_FIXTURES}/Clementi 36.mid" "${URSATZ_REFERENCE_FIXTURES}/Mozart 545.mid"
     DESTINATION ${URSATZ_CORPORA_DIR}/sonatinas)
file(COPY "${URSATZ_REFERENCE_FIXTURES}/Bach 114.mid" "${URSATZ_REFERENCE_FIXTURES}/Clementi 36.mid"
     DESTINATION ${URSATZ_CORPORA_DIR}/small)

# Over 10 s in the fast set (about 122 s of CPU measured): runs nightly.
# Tests sharing a work directory take a RESOURCE_LOCK named after it, so parallel runs do not collide.
ursatz_test(test_analyzer_corpus SOURCES test_corpus.c LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_CORPORA_DIR="${URSATZ_CORPORA_DIR}"
    URSATZ_CORPUS_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/corpus_work"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots" LABELS golden slow PROPERTIES RESOURCE_LOCK corpus_work)

ursatz_test(test_cli_corpus COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz>
                 -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                 -DFIXTURE=${URSATZ_REFERENCE_FIXTURES}/Field\ 1.mid
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_corpus
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_corpus.cmake)

# Over 10 s in the fast set (about 41 s of CPU measured): runs nightly.
if(URSATZ_JSONSCHEMA_PYTHON)
    ursatz_test(test_corpus_index_schema COMMAND ${CMAKE_COMMAND}
                     -DCLI=$<TARGET_FILE:ursatz>
                     -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
                     -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                     -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/corpus-index.v2.schema.json
                     -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                     -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/corpus_index_schema
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_corpus_index.cmake
        LABELS schema slow)
endif()

ursatz_test_executable(test_style_piece LIBS ursatz_analyzer_engine analyzer_test_support cjson ursatz_chain
    INCLUDES ${PROJECT_SOURCE_DIR}/src)
ursatz_test(test_style_piece_smoke TARGET test_style_piece ARGS smoke)
# Over 10 s in the fast set (the five pieces): runs nightly.
ursatz_test(test_style_piece TARGET test_style_piece LABELS slow)

# Over 10 s in the fast set (about 84 s of CPU measured): runs nightly.
ursatz_test(test_style_profile LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_CORPORA_DIR="${URSATZ_CORPORA_DIR}"
    URSATZ_STYLE_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/style_work" PROJECT_VERSION_FOR_TESTS="${PROJECT_VERSION}"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots" LABELS golden slow PROPERTIES RESOURCE_LOCK style_work)

# The smoke profiles the corpus "small" (Bach 114 and Clementi 36).
ursatz_test(test_cli_style_profile_smoke COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz>
                 -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                 -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
                 -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                 -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/style-model.v1.schema.json
                 -DSMOKE=1
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_style_profile_smoke
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_style_profile.cmake)
# Over 10 s in the fast set (about 85 s of CPU measured): runs nightly.
ursatz_test(test_cli_style_profile COMMAND ${CMAKE_COMMAND}
                 -DCLI=$<TARGET_FILE:ursatz>
                 -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_style_profile
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_style_profile.cmake
    LABELS slow)

# Over 10 s in the fast set (about 43 s of CPU measured): runs nightly.
if(URSATZ_JSONSCHEMA_PYTHON)
    ursatz_test(test_style_model_schema COMMAND ${CMAKE_COMMAND}
                     -DCLI=$<TARGET_FILE:ursatz>
                     -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
                     -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                     -DSCHEMA=${PROJECT_SOURCE_DIR}/schema/style-model.v1.schema.json
                     -DCORPORA_DIR=${URSATZ_CORPORA_DIR}
                     -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/style_model_schema
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/validate_style_model.cmake
        LABELS schema slow)
endif()

# The corpus passages the Composer API takes inline (R-619), over Bach 114 analyzed in the build tree (the smoke's size).
ursatz_test(test_corpus_passages LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_PASSAGES_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/corpus_passages_work" VALGRIND)

ursatz_test_executable(test_similar_material LIBS ursatz_analyzer_engine analyzer_test_support cjson
    DEFINITIONS URSATZ_CORPORA_DIR="${URSATZ_CORPORA_DIR}"
    URSATZ_SIMILAR_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/similar_work"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
# The smoke searches Bach 114 alone.
ursatz_test(test_similar_material_smoke TARGET test_similar_material ARGS smoke PROPERTIES RESOURCE_LOCK similar_work)
# Over 10 s in the fast set (about 44 s of CPU measured): runs nightly.
ursatz_test(test_similar_material TARGET test_similar_material LABELS golden slow PROPERTIES RESOURCE_LOCK similar_work)

ursatz_test_executable(test_meter_check LIBS ursatz_analyzer_engine analyzer_test_support cjson m
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")
ursatz_test(test_meter_check_smoke TARGET test_meter_check ARGS smoke)
# Over 10 s in the fast set (about 39 s of CPU measured): runs nightly.
ursatz_test(test_meter_check TARGET test_meter_check LABELS slow)

ursatz_test(test_motive_links LIBS ursatz_analyzer_engine analyzer_test_support cjson INCLUDES ${PROJECT_SOURCE_DIR}/src
    DEFINITIONS URSATZ_LINKS_DIR="${URSATZ_FIXTURES_DIR}/links"
    URSATZ_LINKS_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/motive_links_work" PROPERTIES RESOURCE_LOCK motive_links_work)
if(URSATZ_VALGRIND)
    ursatz_test(test_motive_links_valgrind COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS}
        --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp $<TARGET_FILE:test_motive_links> LABELS slow valgrind
        PROPERTIES RESOURCE_LOCK motive_links_work)
endif()

ursatz_test(test_staleness LIBS ursatz_test_support ursatz_analyzer_engine cjson
    DEFINITIONS URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
if(URSATZ_VALGRIND)
    ursatz_test(test_staleness_valgrind COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS}
        --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp $<TARGET_FILE:test_staleness> LABELS slow valgrind)
endif()
    # Over 10 s in the fast set (about 84 s of CPU measured): runs nightly.

ursatz_test(test_analyzer_version SOURCES test_version.c LIBS ursatz_test_support ursatz_analyzer_engine)

# Consumer isolation (A20261104-31): a target linking only ursatz_analyzer_engine sees the chain's result header but cannot include a chain
# internal header. The leak target must fail to compile, so it is left out of the default build and built by
# consumer_isolation.cmake.
add_library(analyzer_consumer_plain OBJECT consumer/plain.c)
target_link_libraries(analyzer_consumer_plain PRIVATE ursatz_analyzer)
add_library(analyzer_consumer_leak_chain OBJECT EXCLUDE_FROM_ALL consumer/leak_chain.c)
target_link_libraries(analyzer_consumer_leak_chain PRIVATE ursatz_analyzer)
ursatz_test(test_consumer_isolation COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/consumer_isolation.cmake)


ursatz_test(test_text_export LIBS ursatz_test_support ursatz_analyzer_engine
    DEFINITIONS URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml")

ursatz_test_executable(test_text_export_snapshots LIBS ursatz_analyzer_engine analyzer_test_support
    DEFINITIONS URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots")
ursatz_test(test_text_export_snapshots_smoke TARGET test_text_export_snapshots ARGS smoke LABELS golden)
# Over 10 s in the fast set (about 19 s of CPU measured): runs nightly.
ursatz_test(test_text_export_snapshots TARGET test_text_export_snapshots LABELS golden slow)

ursatz_test(test_material_export LIBS ursatz_analyzer_engine analyzer_test_support
    DEFINITIONS URSATZ_ANALYZER_EXPORT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/export"
    URSATZ_MUSICXML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/musicxml"
    URSATZ_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots" LABELS golden)

# The layers of Mozart 545 (R-630): the CLI's --layers folder and Michelle's reading beside the Analyzer's (the full test runs the CLI and
# analyzes the piece: nightly; the smoke reads only the stored fixture).
ursatz_test_executable(test_layers_two_readings LIBS ursatz_analyzer_engine analyzer_test_support
    DEFINITIONS URSATZ_ANALYZER_CLI="$<TARGET_FILE:ursatz>")
add_dependencies(test_layers_two_readings ursatz)
ursatz_test(test_layers_two_readings_smoke TARGET test_layers_two_readings ARGS smoke)
ursatz_test(test_layers_two_readings TARGET test_layers_two_readings LABELS slow)

# The model plan of Mozart 545 (R-635): --model-plan on a copy, the plan validated against its schema.
ursatz_test(cli_model_plan COMMAND ${CMAKE_COMMAND} -DANALYZER=$<TARGET_FILE:ursatz> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_model_plan
                     "-DMIDI=${CMAKE_SOURCE_DIR}/tests/fixtures/framework-v1-corpus/Mozart 545.mid"
                     -DSCHEMA=${CMAKE_SOURCE_DIR}/library/plan/schema/plan.v6.schema.json
                     -DFRAGMENT=${CMAKE_SOURCE_DIR}/library/plan/schema/section.schema.json
                     -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON} -DVALIDATOR=${CMAKE_SOURCE_DIR}/tests/support/validate_schema.py
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_model_plan.cmake)

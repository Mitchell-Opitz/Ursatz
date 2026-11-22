# Root tests: checks that hold for the whole tree.
ursatz_test(test_asserts_on)

# Every include follows cmake/layer-table.txt (spec/target-architecture.md 12.2).
ursatz_test(layer_table COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/check_layer_table.py ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/library/public/manifest.txt)

# No copy of a shared primitive (a gcd, a 128-bit integer, a rational type, the pitch and Roman numeral names, the scale steps) outside its owner
# (tools/ci/check-primitives.sh, R-622); what stays is in tools/ci/primitive-allow.txt.
ursatz_test(primitive_owners COMMAND bash ${CMAKE_SOURCE_DIR}/tools/ci/check-primitives.sh)
# Each application library exports only the names of its export file (R-640).
if(URSATZ_NM AND URSATZ_LOCALIZE_EXPORTS)
    ursatz_test(app_exports COMMAND ${CMAKE_COMMAND} -DNM=${URSATZ_NM}
        "-DCHECKS=$<TARGET_FILE:ursatz_analyzer>|${CMAKE_SOURCE_DIR}/analyzer/src/exports.txt;$<TARGET_FILE:ursatz_composer>|${CMAKE_SOURCE_DIR}/composer/src/exports.txt;$<TARGET_FILE:ursatz_workshop>|${CMAKE_SOURCE_DIR}/workshop/src/exports.txt"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_exports.cmake)
else()
    message(STATUS "Ursatz tests: app_exports is not registered (needs nm and URSATZ_LOCALIZE_EXPORTS)")
endif()

# Test support every part shares: checks, files, temp paths and JSON (tests/support/ursatz_test.h), and the HTTP client of
# the GUI backend tests (tests/support/test_http.h). A part's own helpers stay in its tests/support folder.
ursatz_test_library(ursatz_test_support SOURCES support/ursatz_test.c LIBS cjson
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/support)
ursatz_test_library(ursatz_test_http SOURCES support/test_http.c LIBS ursatz_test_support cjson)
if(WIN32)
    target_link_libraries(ursatz_test_http PUBLIC ws2_32)
endif()

# Every knowledge entry follows its kind's schema and the rules around ids, citations, relations and examples
# (tools/ci/check-knowledge.py, target architecture 5). Needs jsonschema, like the other schema tests.
if(URSATZ_JSONSCHEMA_PYTHON)
    ursatz_test(knowledge_validate COMMAND ${URSATZ_JSONSCHEMA_PYTHON} ${CMAKE_SOURCE_DIR}/tools/ci/check-knowledge.py
        ${CMAKE_SOURCE_DIR}/knowledge LABELS schema)

    # The check itself: one valid set of entries passes, and each of nine broken sets fails with its own message.
    ursatz_test(knowledge_validate_fixtures COMMAND ${CMAKE_COMMAND}
        -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
        -DCHECK=${CMAKE_SOURCE_DIR}/tools/ci/check-knowledge.py
        -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/knowledge
        -DSCHEMA=${CMAKE_SOURCE_DIR}/knowledge/schema
        -P ${CMAKE_CURRENT_SOURCE_DIR}/knowledge_fixtures.cmake LABELS schema)
endif()

# Seeding from a corpus (composer/docs/style.md; R-619). The Analyzer finds the passages of a corpus like a passage or like numerals and the
# Composer seeds from them given inline, so this test needs both apps. "sonatinas" is Clementi 36 from the root reference corpus and the
# Composer's own Mozart 545 (composer/tests/fixtures/corpus-sonatinas), copied into the build tree at configure time and analyzed by
# ursatz corpus at build time; "empty" holds one file the Analyzer cannot import, so nothing in its output is analyzed.
set(SEED_CORPORA_DIR ${CMAKE_CURRENT_BINARY_DIR}/corpora)
set(SEED_REFERENCE ${PROJECT_SOURCE_DIR}/composer/tests/fixtures/corpus-sonatinas)
file(COPY "${URSATZ_FIXTURES_DIR}/framework-v1-corpus/Clementi 36.mid" "${SEED_REFERENCE}/Mozart 545.mid"
     DESTINATION ${SEED_CORPORA_DIR}/sonatinas)
file(WRITE ${SEED_CORPORA_DIR}/empty/broken.mid "not a midi file")
add_custom_command(
    OUTPUT ${SEED_CORPORA_DIR}/sonatinas-analyzed/corpus-index.json ${SEED_CORPORA_DIR}/empty-analyzed/corpus-index.json
    COMMAND ursatz corpus ${SEED_CORPORA_DIR}/sonatinas --out ${SEED_CORPORA_DIR}/sonatinas-analyzed
    COMMAND ursatz corpus ${SEED_CORPORA_DIR}/empty --out ${SEED_CORPORA_DIR}/empty-analyzed
    DEPENDS ursatz
    COMMENT "Analyzing the test corpora for corpus seeding"
    VERBATIM)
add_custom_target(seed_corpora DEPENDS ${SEED_CORPORA_DIR}/sonatinas-analyzed/corpus-index.json)

ursatz_test_executable(test_corpus_seed LIBS ursatz_composer_internal ursatz_analyzer composer_test_support ursatz_test_support
    DEFINITIONS COMPOSER_CLI_PATH="$<TARGET_FILE:ursatz>"
    COMPOSER_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/composer/tests/fixtures"
    COMPOSER_SONATINAS_CORPUS="${SEED_CORPORA_DIR}/sonatinas-analyzed"
    COMPOSER_EMPTY_CORPUS="${SEED_CORPORA_DIR}/empty-analyzed")
add_dependencies(test_corpus_seed ursatz seed_corpora)
# Each mode runs in a directory of its own: the modes write files of the same names, and ctest -j runs them together.
# Slow: progression about 32 s at -j4 (2026-11-21); progression-smoke about 12 s and cli 10 s are kept in the fast set, same 2 s is fast.
# progression searches the corpus twice and fills seeds 1 to 5 and is slow; progression-smoke places a progression and fills seed 1.
foreach(mode motive same plan progression progression-smoke no-match cli)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_seed_${mode})
    set(labels)
    if(mode STREQUAL "progression")
        set(labels slow)
    endif()
    set(test_name test_corpus_seed_${mode})
    if(mode STREQUAL "progression-smoke")
        set(test_name test_corpus_seed_progression_smoke)
    endif()
    ursatz_test(${test_name} TARGET test_corpus_seed ARGS ${mode} LABELS ${labels}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_seed_${mode})
endforeach()

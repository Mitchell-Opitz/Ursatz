# Root tests: checks that hold for the whole tree.
ursatz_test(test_asserts_on)

# Every include follows cmake/layer-table.txt (spec/target-architecture.md 12.2).
ursatz_test(layer_table COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/check_layer_table.py ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/library/public/manifest.txt)

# No copy of a shared primitive (a gcd, a 128-bit integer, a rational type, the pitch and Roman numeral names, the scale steps) outside its owner
# (tools/ci/check-primitives.sh, R-622); what stays is in tools/ci/primitive-allow.txt.
ursatz_test(primitive_owners COMMAND bash ${CMAKE_SOURCE_DIR}/tools/ci/check-primitives.sh)
# The checks CI used to run as steps (tools/ci), run by every ctest: the docs of each part (README and doc sizes, links, anchors, index coverage)
# and of the root folder alone,
# the statics and hearing boundaries and the frontend theory. The reach scan builds its own tree, so it is slow (CI keeps its own step with ccache).
foreach(part library analyzer composer gui workshop common cli knowledge tools tests)
    ursatz_test(docs_${part} COMMAND bash ${CMAKE_SOURCE_DIR}/tools/ci/check-docs.sh ${part})
endforeach()
ursatz_test(docs_root COMMAND bash ${CMAKE_SOURCE_DIR}/tools/ci/check-docs.sh .)
ursatz_test(statics COMMAND bash ${CMAKE_SOURCE_DIR}/tools/ci/check-statics.sh analyzer composer)
ursatz_test(hearing_boundary COMMAND bash ${CMAKE_SOURCE_DIR}/tools/ci/check-hearing.sh)
ursatz_test(frontend_theory COMMAND bash ${CMAKE_SOURCE_DIR}/tools/ci/check-frontend-theory.sh)
ursatz_test(reach COMMAND bash ${CMAKE_SOURCE_DIR}/tools/ci/check-reach.sh ${CMAKE_BINARY_DIR}/reach-scan LABELS slow)
# The layer table and primitive checks, each on a temporary tree with a planted violation (tests/check_self_test.py): the check fails on it and passes without it.
ursatz_test(check_layer_table_self_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_self_test.py layer ${CMAKE_SOURCE_DIR})
ursatz_test(check_primitives_self_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_self_test.py primitives ${CMAKE_SOURCE_DIR})
# No two test sources use one port (ctest -j runs them together): a port literal from 18000 to 18999 appears in one file.
ursatz_test(test_ports_unique COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_test_ports.py ${CMAKE_SOURCE_DIR})
# The goldens update script (cmake/UpdateGoldens.cmake) fails when the goldens it wrote do not pass.
ursatz_test(update_goldens_script COMMAND ${CMAKE_COMMAND} -DSCRIPT=${CMAKE_SOURCE_DIR}/cmake/UpdateGoldens.cmake
    -DWORK=${CMAKE_CURRENT_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/update_goldens_test.cmake)
# Each application library exports only the names of its export file (R-640).
if(URSATZ_NM AND URSATZ_LOCALIZE_EXPORTS)
    ursatz_test(app_exports COMMAND ${CMAKE_COMMAND} -DNM=${URSATZ_NM}
        "-DCHECKS=$<TARGET_FILE:ursatz_analyzer>|${CMAKE_SOURCE_DIR}/analyzer/src/exports.txt;$<TARGET_FILE:ursatz_composer>|${CMAKE_SOURCE_DIR}/composer/src/exports.txt;$<TARGET_FILE:ursatz_workshop>|${CMAKE_SOURCE_DIR}/workshop/src/exports.txt"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_exports.cmake)
    # The check itself: a library with two functions, an export file that lists both, one that leaves one out and one that names another.
    ursatz_test_library(exports_self_test SOURCES self_test/exports_self_test.c)
    ursatz_test(check_exports_self_test COMMAND ${CMAKE_COMMAND} -DNM=${URSATZ_NM} -DLIBRARY=$<TARGET_FILE:exports_self_test>
        -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_exports.cmake -DWORK=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_exports_self_test.cmake)
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

    # The check itself: one valid set of entries passes, and each broken set fails with its own message.
    ursatz_test(knowledge_validate_fixtures COMMAND ${CMAKE_COMMAND}
        -DPYTHON=${URSATZ_JSONSCHEMA_PYTHON}
        -DCHECK=${CMAKE_SOURCE_DIR}/tools/ci/check-knowledge.py
        -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/knowledge
        -DSCHEMA=${CMAKE_SOURCE_DIR}/knowledge/json-schema
        -P ${CMAKE_CURRENT_SOURCE_DIR}/knowledge_fixtures.cmake LABELS schema)
endif()

# Seeding from a corpus (composer/docs/style.md; R-619). The Analyzer finds the passages of a corpus like a passage or like numerals and the
# Composer seeds from them given inline, so the test (composer/tests/test_corpus_seed.c) needs both apps. "sonatinas" is Clementi 36 from the root reference corpus and the
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


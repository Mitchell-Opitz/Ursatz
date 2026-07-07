add_ursatz_test(musicxml_export)
add_ursatz_test(musicxml_score)
add_ursatz_test(musicxml_score_reference)
target_compile_definitions(test_musicxml_score_reference PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_ursatz_test(musicxml_score_overlaps)
target_compile_definitions(test_musicxml_score_overlaps PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus"
    URSATZ_MUSICXML_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/musicxml")
add_ursatz_test(musicxml_score_errors)
add_ursatz_test(musicxml_score_fixtures)
target_compile_definitions(test_musicxml_score_fixtures PRIVATE
    URSATZ_MUSICXML_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/musicxml")

add_executable(test_musicxml_export_mozart545 test_musicxml_export_mozart545.c)
target_link_libraries(test_musicxml_export_mozart545 PRIVATE ursatz)
add_test(NAME musicxml_export_mozart545 COMMAND test_musicxml_export_mozart545
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_musicxml_export_bach_prelude test_musicxml_export_bach_prelude.c)
target_link_libraries(test_musicxml_export_bach_prelude PRIVATE ursatz)
add_test(NAME musicxml_export_bach_prelude COMMAND test_musicxml_export_bach_prelude
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_ursatz_architecture_test(export)

# Scoped memcheck coverage for this module's own test binaries only (not a repo-wide valgrind mechanism -- see
# docs/modules/export.md). Skips silently if valgrind isn't installed.
find_program(URSATZ_EXPORT_VALGRIND valgrind)
if(URSATZ_EXPORT_VALGRIND)
  foreach(export_test_name musicxml_export musicxml_score musicxml_score_errors musicxml_score_fixtures musicxml_score_overlaps musicxml_score_reference)
    add_test(NAME ${export_test_name}_valgrind
             COMMAND ${URSATZ_EXPORT_VALGRIND} --leak-check=full --error-exitcode=1
                     $<TARGET_FILE:test_${export_test_name}>
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endforeach()
endif()

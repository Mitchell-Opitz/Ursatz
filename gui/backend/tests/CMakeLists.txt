# Every test links ursatz_gui_core (backend/CMakeLists.txt) and runs in its own working directory.
function(ursatz_gui_add_test name source workdir)
  add_executable(test_${name} ${source})
  target_link_libraries(test_${name} PRIVATE ursatz_gui_core)
  file(MAKE_DIRECTORY ${workdir})
  add_test(NAME ${name} COMMAND test_${name} WORKING_DIRECTORY ${workdir})
endfunction()

ursatz_gui_add_test(health_route test_health_route.c ${CMAKE_CURRENT_BINARY_DIR}/health_route)

ursatz_gui_add_test(analyzer_route test_analyzer_route.c ${CMAKE_CURRENT_BINARY_DIR}/analyzer_route)
# Slow: about 363 s in a -j4 run (R-307).
set_tests_properties(analyzer_route PROPERTIES LABELS slow)
# The test compares the stored analysis with what the Analyzer CLI writes for the same file, in its own
# working directory (pieces/ and cli/ are created there).
add_dependencies(test_analyzer_route ursatz-analyzer)
target_compile_definitions(test_analyzer_route PRIVATE
  URSATZ_GUI_TEST_URSATZ_DIR="${CMAKE_SOURCE_DIR}/external/ursatz"
  URSATZ_GUI_TEST_ANALYZER_CLI="$<TARGET_FILE:ursatz-analyzer>"
)

ursatz_gui_add_test(corpus_route test_corpus_route.c ${CMAKE_CURRENT_BINARY_DIR}/corpus_route)
# The test creates collections/, pieces/ and data/ in its own working directory.

ursatz_gui_add_test(piece_metadata test_piece_metadata.c ${CMAKE_CURRENT_BINARY_DIR}/piece_metadata)

ursatz_gui_add_test(id_service test_id_service.c ${CMAKE_CURRENT_BINARY_DIR}/id_service)

ursatz_gui_add_test(job_queue test_job_queue.c ${CMAKE_CURRENT_BINARY_DIR}/job_queue)
# The test cancels a theme compose made from a request fixture; no server is started.
target_compile_definitions(test_job_queue PRIVATE
  URSATZ_GUI_TEST_THEME_REQUEST="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/composer/theme-request.json"
)

ursatz_gui_add_test(composer_route test_composer_route.c ${CMAKE_CURRENT_BINARY_DIR}/composer_route)
# The test asks the linked Composer for its versions and its vocabulary (which it compares with the frontend's fixture),
# in its own working directory.
target_compile_definitions(test_composer_route PRIVATE
  URSATZ_GUI_TEST_VOCABULARY_FIXTURE="${CMAKE_SOURCE_DIR}/frontend/tests/fixtures/vocabulary.json"
)

ursatz_gui_add_test(styles_route test_styles_route.c ${CMAKE_CURRENT_BINARY_DIR}/styles_route)
# Slow: about 260 s in a -j4 run (R-307).
set_tests_properties(styles_route PROPERTIES LABELS slow)
# The test builds styles from the reference pieces; it creates pieces/, collections/ and styles/ in its own
# working directory.
target_compile_definitions(test_styles_route PRIVATE
  URSATZ_GUI_TEST_URSATZ_DIR="${CMAKE_SOURCE_DIR}/external/ursatz"
)

ursatz_gui_add_test(material_service test_material_service.c ${CMAKE_CURRENT_BINARY_DIR}/material_service)
# The test creates its own materials_<pid>/ store in the build tree (URSATZ_GUI_TEST_STORE_DIR) and removes it.
target_compile_definitions(test_material_service PRIVATE
  URSATZ_GUI_TEST_STORE_DIR="${CMAKE_CURRENT_BINARY_DIR}/material_service"
)

ursatz_gui_add_test(subject_service test_subject_service.c ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
# The test creates its own materials_<pid>/, subjects_<pid>/ and pieces/ in its working directory.

ursatz_gui_add_test(materials_route test_materials_route.c ${CMAKE_CURRENT_BINARY_DIR}/materials_route)
# The test creates materials/, subjects/ and pieces/ in its own working directory and reads the replies the Composer
# gave as a command (fixtures/composer).
target_compile_definitions(test_materials_route PRIVATE
  URSATZ_GUI_TEST_COMPOSER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/composer"
  URSATZ_GUI_TEST_PLAYBACK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/playback"
)

ursatz_gui_add_test(subjects_route test_subjects_route.c ${CMAKE_CURRENT_BINARY_DIR}/subjects_route)
# The test creates materials/, subjects/ and pieces/ in its own working directory.
target_compile_definitions(test_subjects_route PRIVATE
  URSATZ_GUI_TEST_URSATZ_DIR="${CMAKE_SOURCE_DIR}/external/ursatz"
)

ursatz_gui_add_test(link_service test_link_service.c ${CMAKE_CURRENT_BINARY_DIR}/link_service)
# The test creates pieces/, collections/, materials/, subjects/ and links/ in its own working directory and reads the
# link fixtures.
target_compile_definitions(test_link_service PRIVATE
  URSATZ_GUI_TEST_LINK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/links"
)

ursatz_gui_add_test(links_route test_links_route.c ${CMAKE_CURRENT_BINARY_DIR}/links_route)
# The test creates pieces/, collections/, materials/, subjects/ and links/ in its own working directory.
target_compile_definitions(test_links_route PRIVATE
  URSATZ_GUI_TEST_LINK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/links"
)

ursatz_gui_add_test(theme_route test_theme_route.c ${CMAKE_CURRENT_BINARY_DIR}/theme_route)
# Slow: every theme the real Composer composes now measures its developments (Composer API 1.10, about 1 s a theme), which
# took the test to about 30 s (R-307).
set_tests_properties(theme_route PROPERTIES LABELS slow)
# The test composes with the Composer API in its own working directory (materials/, subjects/ and pieces/ are created
# there), uploads the link fixtures for the brief and compares the files with fixtures/composer/b6_compose.
target_compile_definitions(test_theme_route PRIVATE
  URSATZ_GUI_TEST_LINK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/links"
)

ursatz_gui_add_test(piece_route test_piece_route.c ${CMAKE_CURRENT_BINARY_DIR}/piece_route)
# Slow: about 279 s in a -j4 run (R-307).
set_tests_properties(piece_route PROPERTIES LABELS slow)
# The test composes with the Composer API in its own working directory (materials/, subjects/ and pieces/ are created
# there), uploads the link fixtures for the brief and compares the files with fixtures/composer/b6_compose.
target_compile_definitions(test_piece_route PRIVATE
  URSATZ_GUI_TEST_LINK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/links"
)

ursatz_gui_add_test(piece_snapshot test_piece_snapshot.c ${CMAKE_CURRENT_BINARY_DIR}/piece_snapshot)
# The test compares the detail note ids, score and playback replies of three pieces with fixtures/pieces; it creates
# pieces/ in its own working directory. URSATZ_GUI_SNAPSHOT_CAPTURE=1 rewrites the snapshot.
target_compile_definitions(test_piece_snapshot PRIVATE
  URSATZ_GUI_TEST_URSATZ_DIR="${CMAKE_SOURCE_DIR}/external/ursatz"
  URSATZ_GUI_TEST_PIECE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/pieces"
)

ursatz_gui_add_test(piece_lock test_piece_lock.c ${CMAKE_CURRENT_BINARY_DIR}/piece_lock)
# The test makes pieces/ in its own working directory and uploads this MusicXML file as the piece.
target_compile_definitions(test_piece_lock PRIVATE
  URSATZ_GUI_TEST_SOURCE="${CMAKE_SOURCE_DIR}/external/ursatz/tests/fixtures/musicxml/chords_backup.musicxml"
)

ursatz_gui_add_test(stale_analysis test_stale_analysis.c ${CMAKE_CURRENT_BINARY_DIR}/stale_analysis)
# The test creates pieces/, styles/, collections/, materials/, subjects/ and links/ in its own working directory and reads
# the link fixtures.
target_compile_definitions(test_stale_analysis PRIVATE
  URSATZ_GUI_TEST_LINK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/links"
)

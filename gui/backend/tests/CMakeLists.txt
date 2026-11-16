# Every test links ursatz_gui_core (backend/CMakeLists.txt) and the shared test support (tests/support: ursatz_test_support,
# or ursatz_test_http for the tests that talk to a server or sleep), and runs in its own working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/analyzer_route)
ursatz_test(analyzer_route LIBS ursatz_gui_core ursatz_test_http DEFINITIONS URSATZ_GUI_TEST_ANALYZER_CLI="$<TARGET_FILE:ursatz-analyzer>" LABELS slow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/analyzer_route)
# Slow: about 363 s in a -j4 run (R-307).
# The test compares the stored analysis with what the Analyzer CLI writes for the same file, in its own
# working directory (pieces/ and cli/ are created there).
add_dependencies(test_analyzer_route ursatz-analyzer)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_route)
ursatz_test(corpus_route LIBS ursatz_gui_core ursatz_test_http WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_route)
# The test creates collections/ and pieces/ in its own working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_metadata)
ursatz_test(piece_metadata LIBS ursatz_gui_core ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_metadata)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/id_service)
ursatz_test(id_service LIBS ursatz_gui_core ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/id_service)
# The test makes one data folder per case in its own working directory and removes it.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data_check)
ursatz_test(data_check LIBS ursatz_gui_core ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data_check)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/job_queue)
ursatz_test(job_queue LIBS ursatz_gui_core ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_THEME_REQUEST="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/composer/theme-request.json"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/job_queue)
# The test cancels a theme compose made from a request fixture; no server is started.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/composer_route)
ursatz_test(composer_route LIBS ursatz_gui_core ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_VOCABULARY_FIXTURE="${PROJECT_SOURCE_DIR}/frontend/tests/fixtures/vocabulary.json"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/composer_route)
# The test asks the linked Composer for its versions and its vocabulary (which it compares with the frontend's fixture),
# in its own working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/styles_route)
ursatz_test(styles_route LIBS ursatz_gui_core ursatz_test_http LABELS slow WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/styles_route)
# Slow: about 260 s in a -j4 run (R-307).
# The test builds styles from the reference pieces; it creates pieces/, collections/ and styles/ in its own
# working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)
ursatz_test(material_service LIBS ursatz_gui_core ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_STORE_DIR="${CMAKE_CURRENT_BINARY_DIR}/material_service"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)
# The test creates its own materials_<pid>/ store in the build tree (URSATZ_GUI_TEST_STORE_DIR) and removes it.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
ursatz_test(subject_service LIBS ursatz_gui_core ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
# The test creates its own materials_<pid>/, subjects_<pid>/ and pieces/ in its working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/materials_route)
ursatz_test(materials_route LIBS ursatz_gui_core ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_COMPOSER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/composer"
    URSATZ_GUI_TEST_EXPORT_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/export"
    URSATZ_GUI_TEST_PLAYBACK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/playback"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/materials_route)
# The test creates materials/, subjects/ and pieces/ in its own working directory. It reads the GUI-computed defaults
# (fixtures/composer/b11_captures.json), the motif playback captures (fixtures/playback) and the one-bar score the export
# test installs as a composed version's score (fixtures/export).

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subjects_route)
ursatz_test(subjects_route LIBS ursatz_gui_core ursatz_test_http WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subjects_route)
# The test creates materials/, subjects/ and pieces/ in its own working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/link_service)
ursatz_test(link_service LIBS ursatz_gui_core ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/link_service)
# The test creates pieces/, collections/, materials/, subjects/ and links/ in its own working directory and reads the
# link fixtures.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/links_route)
ursatz_test(links_route LIBS ursatz_gui_core ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/links_route)
# The test creates pieces/, collections/, materials/, subjects/ and links/ in its own working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/theme_route)
ursatz_test(theme_route LIBS ursatz_gui_core ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links" LABELS slow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/theme_route)
# Slow: every theme the real Composer composes now measures its developments (Composer API 1.10, about 1 s a theme), which
# took the test to about 30 s (R-307).
# The test composes with the Composer API in its own working directory (materials/, subjects/ and pieces/ are created
# there), uploads the link fixtures for the brief and checks the stored files and the served playback (version_check.h).

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_route)
ursatz_test(piece_route LIBS ursatz_gui_core ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links" LABELS slow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_route)
# Slow: about 279 s in a -j4 run (R-307).
# The test composes with the Composer API in its own working directory (materials/, subjects/ and pieces/ are created
# there), uploads the link fixtures for the brief and checks the stored files and the served playback (version_check.h).

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_snapshot)
ursatz_test(piece_snapshot LIBS ursatz_gui_core ursatz_test_http DEFINITIONS URSATZ_GUI_TEST_PIECE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/pieces"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_snapshot)
# The test compares the detail note ids, score and playback replies of three pieces with fixtures/pieces; it creates
# pieces/ in its own working directory. URSATZ_GUI_SNAPSHOT_CAPTURE=1 rewrites the snapshot.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_lock)
ursatz_test(piece_lock LIBS ursatz_gui_core ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_SOURCE="${URSATZ_FIXTURES_DIR}/musicxml/chords_backup.musicxml"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_lock)
# The test makes pieces/ in its own working directory and uploads this MusicXML file as the piece.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stale_analysis)
ursatz_test(stale_analysis LIBS ursatz_gui_core ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stale_analysis)
# The test creates pieces/, styles/, collections/, materials/, subjects/ and links/ in its own working directory and reads
# the link fixtures.

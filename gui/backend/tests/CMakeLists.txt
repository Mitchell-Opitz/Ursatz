# Every test links ursatz_gui_core (backend/CMakeLists.txt) and the shared test support (tests/support: ursatz_test_support,
# or ursatz_test_http for the tests that talk to a server or sleep), and runs in its own working directory.

# The frontend's vocabulary.json and forms.json are written at build time by the in-tree Composer (the strings the two
# routes serve), so the frontend tests and composer_route read what the Composer says now. gui/CMakeLists.txt adds this
# folder before frontend/tests, so the variable is set when the frontend tests are registered.
ursatz_test_executable(gui_write_frontend_fixtures SOURCES write_frontend_fixtures.c LIBS ursatz_gui_core_internal)
set(URSATZ_GUI_GENERATED_FIXTURES ${CMAKE_BINARY_DIR}/gui/frontend/tests/fixtures CACHE INTERNAL "")
add_custom_command(
    OUTPUT ${URSATZ_GUI_GENERATED_FIXTURES}/vocabulary.json ${URSATZ_GUI_GENERATED_FIXTURES}/forms.json
           ${URSATZ_GUI_GENERATED_FIXTURES}/piece-ternary.json ${URSATZ_GUI_GENERATED_FIXTURES}/piece-rounded-binary.json
           ${URSATZ_GUI_GENERATED_FIXTURES}/piece-minuet.json ${URSATZ_GUI_GENERATED_FIXTURES}/piece-scherzo.json
           ${URSATZ_GUI_GENERATED_FIXTURES}/piece-variations.json ${URSATZ_GUI_GENERATED_FIXTURES}/piece-figured-variations.json
           ${URSATZ_GUI_GENERATED_FIXTURES}/piece-sonatina.json ${URSATZ_GUI_GENERATED_FIXTURES}/piece-small-rondo-ababa.json
    COMMAND ${CMAKE_COMMAND} -E make_directory ${URSATZ_GUI_GENERATED_FIXTURES}
    COMMAND gui_write_frontend_fixtures ${URSATZ_GUI_GENERATED_FIXTURES}
    DEPENDS gui_write_frontend_fixtures
    COMMENT "Writing the frontend vocabulary and forms fixtures")
add_custom_target(gui_frontend_fixtures ALL
    DEPENDS ${URSATZ_GUI_GENERATED_FIXTURES}/vocabulary.json ${URSATZ_GUI_GENERATED_FIXTURES}/forms.json ${URSATZ_GUI_GENERATED_FIXTURES}/piece-minuet.json)

# Opens a workshop on the test's working directory and starts the server on it (support/server_support.h).
ursatz_test_library(gui_server_support SOURCES support/server_support.c LIBS ursatz_gui_core ursatz_test_http)
ursatz_test_library(gui_server_support_internal SOURCES support/server_support.c LIBS ursatz_gui_core_internal ursatz_test_http)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/analyzer_route)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/analyzer_route_smoke)
ursatz_test_executable(test_analyzer_route LIBS ursatz_gui_core_internal ursatz_test_http gui_server_support_internal)
ursatz_test(analyzer_route TARGET test_analyzer_route LABELS slow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/analyzer_route)
# Slow: about 18 s at -j4 (2026-11-21).
# analyzer_route_smoke (argument smoke, port 18778, its own working directory) uploads Bach 114 alone and runs the checks
# on it that hold for any piece: the detail, the score's meter, the document's sections and the text export.
ursatz_test(analyzer_route_smoke TARGET test_analyzer_route ARGS smoke
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/analyzer_route_smoke)
# The test creates pieces/ in its own working directory. It does not run the Analyzer CLI: the Analyzer's own tests
# check the CLI's output.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_route)
ursatz_test(corpus_route LIBS ursatz_gui_core ursatz_test_http gui_server_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_route)
# The test creates collections/ and pieces/ in its own working directory.

# The one status-to-HTTP table of the routes and the last error the services keep for them (R-639).
ursatz_test(gui_http_status SOURCES test_http_status.c LIBS ursatz_gui_core_internal ursatz_test_support)

# The generic workshop routes (P64 B7): any query or command by its name, and the status, in a working directory of its own.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_routes)
ursatz_test(workshop_routes SOURCES test_workshop_routes.c LIBS ursatz_gui_core ursatz_test_http gui_server_support
    DEFINITIONS URSATZ_GUI_TEST_MIDI="${URSATZ_FIXTURES_DIR}/framework-v1-corpus/Bach 114.mid"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_routes)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/composer_route)
ursatz_test(composer_route LIBS ursatz_gui_core ursatz_test_http gui_server_support
    DEFINITIONS URSATZ_GUI_TEST_VOCABULARY_FIXTURE="${URSATZ_GUI_GENERATED_FIXTURES}/vocabulary.json"
    URSATZ_GUI_TEST_FORMS_FIXTURE="${URSATZ_GUI_GENERATED_FIXTURES}/forms.json"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/composer_route)
# The test asks the linked Composer for its versions, its vocabulary and its forms (which it compares with the files the
# build generated for the frontend tests, gui_frontend_fixtures), in its own working directory.
add_dependencies(test_composer_route gui_frontend_fixtures)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/styles_route)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/styles_route_smoke)
ursatz_test_executable(test_styles_route LIBS ursatz_gui_core_internal ursatz_test_http gui_server_support_internal)
ursatz_test(styles_route TARGET test_styles_route LABELS slow WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/styles_route)
# Slow: about 11 s at -j4 (2026-11-21).
# styles_route_smoke (argument smoke, port 18779, its own working directory) uploads Clementi 36 alone, builds a style from
# it, lists it and reads it back.
ursatz_test(styles_route_smoke TARGET test_styles_route ARGS smoke
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/styles_route_smoke)
# The test builds styles from the reference pieces; it creates pieces/, collections/ and styles/ in its own
# working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/materials_route)
ursatz_test(materials_route LIBS ursatz_gui_core_internal ursatz_test_http gui_server_support_internal
    DEFINITIONS URSATZ_GUI_TEST_COMPOSER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/composer"
    URSATZ_GUI_TEST_EXPORT_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/export"
    URSATZ_GUI_TEST_PLAYBACK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/playback"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/materials_route)
# The test creates materials/, subjects/ and pieces/ in its own working directory. It reads the GUI-computed defaults
# (fixtures/composer/b11_captures.json), the motif playback captures (fixtures/playback) and the one-bar score the export
# test installs as a composed version's score (fixtures/export).

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subjects_route)
ursatz_test(subjects_route LIBS ursatz_gui_core ursatz_test_http gui_server_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subjects_route)
# The test creates materials/, subjects/ and pieces/ in its own working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/links_route)
ursatz_test(links_route LIBS ursatz_gui_core ursatz_test_http gui_server_support
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/links_route)
# The test creates pieces/, collections/, materials/, subjects/ and links/ in its own working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/theme_route)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/theme_route_smoke)
ursatz_test_executable(test_theme_route LIBS ursatz_gui_core_internal ursatz_test_http gui_server_support_internal
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links")
ursatz_test(theme_route TARGET test_theme_route LABELS slow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/theme_route)
# Slow: about 35 s at -j4 (2026-11-21). Every theme the real Composer composes measures its developments (Composer API
# 1.10, about 1 s a theme) (R-307).
# The test composes with the Composer API in its own working directory (materials/, subjects/ and pieces/ are created
# there), uploads the link fixtures for the brief and checks the stored files and the served playback (version_check.h).
# theme_route_smoke (argument smoke, port 18777, its own working directory) saves the motifs and runs the real Composer
# case only.
ursatz_test(theme_route_smoke TARGET test_theme_route ARGS smoke
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/theme_route_smoke)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_route)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_route_smoke)
ursatz_test_executable(test_piece_route LIBS ursatz_gui_core_internal ursatz_test_http gui_server_support_internal
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links")
ursatz_test(piece_route TARGET test_piece_route LABELS slow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_route)
# Slow: about 64 s at -j4 (2026-11-21).
# The test composes with the Composer API in its own working directory (materials/, subjects/ and pieces/ are created
# there), uploads the link fixtures for the brief and checks the stored files and the served playback (version_check.h).
# piece_route_smoke (argument smoke, port 18776, its own working directory) runs the real Composer case only.
ursatz_test(piece_route_smoke TARGET test_piece_route ARGS smoke
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_route_smoke)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_snapshot)
ursatz_test(piece_snapshot LIBS ursatz_gui_core ursatz_test_http gui_server_support
    DEFINITIONS URSATZ_GUI_TEST_PIECE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/pieces"
    URSATZ_GUI_TEST_ANALYZER_SNAPSHOTS="${CMAKE_SOURCE_DIR}/analyzer/tests/snapshots/gui"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_snapshot LABELS golden)
# The test compares the detail note ids, score and playback replies of three pieces with fixtures/pieces (the Bach 114
# playback with the Analyzer's own snapshot); it creates pieces/ in its own working directory.
# URSATZ_UPDATE_GOLDENS=1 rewrites the snapshot in fixtures/pieces, never the Analyzer's.

# The routes on the migrated version 0 fixture (P63 B7): the test copies workshop/tests/fixtures/data-v0 into its working directory, migrates it
# and serves it. The folder it leaves is validated by workshop_formats.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/migrated_routes)
ursatz_test(migrated_routes SOURCES test_migrated_routes.c LIBS ursatz_gui_core_internal ursatz_test_http gui_server_support_internal
    DEFINITIONS URSATZ_WORKSHOP_DATA_V0="${CMAKE_SOURCE_DIR}/workshop/tests/fixtures/data-v0"
    PROPERTIES FIXTURES_SETUP migrated_routes_files RESOURCE_LOCK migrated_routes_files
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/migrated_routes)

# The one parser of the route table (R-647); no server is started.
ursatz_test(route_table SOURCES test_route_table.c LIBS ursatz_gui_core ursatz_test_support)

# The event stream (P63 B13): a raw socket reads GET /api/events while a subject is made and an import runs; a ninth stream is refused; the server stops with
# the streams open.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/events_route)
ursatz_test(gui_events_route SOURCES test_events_route.c LIBS ursatz_gui_core ursatz_test_http gui_server_support
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/events_route)

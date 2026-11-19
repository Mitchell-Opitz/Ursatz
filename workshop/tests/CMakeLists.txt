# The workshop's tests: opening, settings and the data folder lock (P63 B4), and the services (P63 B5). Each runs in its own working
# directory.
ursatz_test(workshop_open LIBS ursatz_workshop)
ursatz_test(workshop_settings LIBS ursatz_workshop)
ursatz_test(workshop_lock LIBS ursatz_workshop)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_metadata)
ursatz_test(piece_metadata LIBS ursatz_workshop ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_metadata)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/id_service)
ursatz_test(id_service LIBS ursatz_workshop ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/id_service)

# The migration of the GUI's data folder to layout 1, its refusals and the data check's cases (P63 B7). The test copies the version 0
# fixture into folders of its own working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_migrate)
ursatz_test(workshop_migrate SOURCES test_migrate.c LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_DATA_V0="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/data-v0"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_migrate)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/job_queue)
ursatz_test(job_queue LIBS ursatz_workshop ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_THEME_REQUEST="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/composer/theme-request.json"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/job_queue)
# The test cancels a theme compose made from a request fixture; no server is started.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)
ursatz_test(material_service LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_STORE_DIR="${CMAKE_CURRENT_BINARY_DIR}/material_service"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)
# The test creates its own materials_<pid>/ folder in the build tree (URSATZ_GUI_TEST_STORE_DIR), works in it as its data folder, and removes it.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
ursatz_test(subject_service LIBS ursatz_workshop ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
# The test creates its own projects/, subjects_<pid>/ and library/pieces/ in its working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/link_service)
ursatz_test(link_service LIBS ursatz_workshop ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/link_service)
# The test creates pieces/, collections/, materials/, subjects/ and links/ in its own working directory and reads the
# link fixtures.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_lock)
ursatz_test(piece_lock LIBS ursatz_workshop ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_SOURCE="${URSATZ_FIXTURES_DIR}/musicxml/chords_backup.musicxml"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_lock)
# The test makes pieces/ in its own working directory and uploads this MusicXML file as the piece.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stale_analysis)
ursatz_test(stale_analysis LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stale_analysis)
# The test creates pieces/, styles/, collections/, materials/, subjects/ and links/ in its own working directory and reads
# the link fixtures.


file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_jobs)
ursatz_test(workshop_jobs SOURCES test_jobs.c LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_jobs)
# The test creates library/, settings/, cache/ and the stores in its own working directory, reads the link fixtures, and cancels link runs
# and style builds between pieces.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_variants)
ursatz_test(workshop_variants SOURCES test_variants.c LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_STORE_DIR="${CMAKE_CURRENT_BINARY_DIR}/workshop_variants"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_variants)
# The test creates its own materials_<pid>/ store in the build tree (URSATZ_GUI_TEST_STORE_DIR), saves a motif and three generations of
# variants through the workshop functions, and removes it.

# Every file the layout names, in the folder the migrated_routes test migrates and then writes into (the routes make a project, an item,
# a version, a mark and a subject, and compose the theme again), against its schema in workshop/schema. Needs python3 with jsonschema
# (cmake/UrsatzTools.cmake); it runs after migrated_routes.
if(URSATZ_JSONSCHEMA_PYTHON)
    ursatz_test(workshop_formats COMMAND ${URSATZ_JSONSCHEMA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/validate_formats.py
                     ${PROJECT_SOURCE_DIR}/workshop/schema ${CMAKE_BINARY_DIR}/gui/backend/tests/migrated_routes
        LABELS schema
        PROPERTIES FIXTURES_REQUIRED migrated_routes_files)
endif()

# Recovery after a stop in the middle of a change (P63 B8): hand-made leftovers of every kind, recovered by workshop_open from the journal.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_recovery)
ursatz_test(workshop_recovery SOURCES test_recovery.c LIBS ursatz_workshop WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_recovery)

# A stop at every step of every kind of change, in a forked child, and what the folder holds after the open recovers it (P63 B8). Each case copies one base
# folder, so the test makes its own folders in its working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_crash_points)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_crash_points_smoke)
ursatz_test_executable(test_workshop_crash_points SOURCES test_crash_points.c LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_TEST_HOOKS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links")
ursatz_test(workshop_crash_points TARGET test_workshop_crash_points LABELS slow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_crash_points)
# Slow: about 35 s (a Library piece is analyzed and a theme composed at each stop). The smoke run (argument smoke) stops the item commands alone.
ursatz_test(workshop_crash_points_smoke TARGET test_workshop_crash_points ARGS smoke
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_crash_points_smoke)

# The layers a piece and a composed theme keep (P63 B9): Mozart 545's MIDI imported, a theme composed, a reading of another digest refused. The test makes
# its own data folder in its working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_layers)
ursatz_test(workshop_layers SOURCES test_layers.c LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_TEST_MOZART="${URSATZ_FIXTURES_DIR}/framework-v1-corpus/Mozart 545.mid"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_layers)

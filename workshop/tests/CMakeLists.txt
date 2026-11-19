# The workshop's tests: opening, settings and the data folder lock (P63 B4), and the services (P63 B5). Each runs in its own working
# directory.
ursatz_test(workshop_open LIBS ursatz_workshop)
ursatz_test(workshop_settings LIBS ursatz_workshop)
ursatz_test(workshop_lock LIBS ursatz_workshop)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_metadata)
ursatz_test(piece_metadata LIBS ursatz_workshop ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_metadata)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/id_service)
ursatz_test(id_service LIBS ursatz_workshop ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/id_service)

# The test makes one data folder per case in its own working directory and removes it.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data_check)
ursatz_test(data_check LIBS ursatz_workshop ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data_check)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/job_queue)
ursatz_test(job_queue LIBS ursatz_workshop ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_THEME_REQUEST="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/composer/theme-request.json"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/job_queue)
# The test cancels a theme compose made from a request fixture; no server is started.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)
ursatz_test(material_service LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_STORE_DIR="${CMAKE_CURRENT_BINARY_DIR}/material_service"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)
# The test creates its own materials_<pid>/ store in the build tree (URSATZ_GUI_TEST_STORE_DIR) and removes it.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gui_material_migration)
ursatz_test(gui_material_migration SOURCES test_material_migration.c LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_STORE_DIR="${CMAKE_CURRENT_BINARY_DIR}/gui_material_migration"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gui_material_migration)
# The test reads old motif, theme and piece files (the key text and the mode words), composes them and saves them again with
# the Composer API in its own materials_<pid>/ store in the build tree (URSATZ_GUI_TEST_STORE_DIR), and removes it.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
ursatz_test(subject_service LIBS ursatz_workshop ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
# The test creates its own materials_<pid>/, subjects_<pid>/ and pieces/ in its working directory.

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


file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_variants)
ursatz_test(workshop_variants SOURCES test_variants.c LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_STORE_DIR="${CMAKE_CURRENT_BINARY_DIR}/workshop_variants"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_variants)
# The test creates its own materials_<pid>/ store in the build tree (URSATZ_GUI_TEST_STORE_DIR), saves a motif and three generations of
# variants through the workshop functions, and removes it.

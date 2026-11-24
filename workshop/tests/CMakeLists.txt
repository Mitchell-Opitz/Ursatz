# The workshop's tests: opening, settings and the data folder lock (P63 B4), and the services (P63 B5). Each runs in its own working
# directory.
ursatz_test(workshop_open LIBS ursatz_workshop)
ursatz_test(workshop_user_knowledge SOURCES test_user_knowledge.c LIBS ursatz_workshop_internal
    DEFINITIONS URSATZ_USER_KNOWLEDGE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/knowledge-user/voicings.json")
ursatz_test(workshop_config LIBS ursatz_workshop)
ursatz_test(workshop_lock LIBS ursatz_workshop)

# The third call, tools (P64 B1): a workshop with no data folder and the tools the command line calls. The seed corpus is built by the target
# seed_corpora (tests/CMakeLists.txt); the test works in a folder of its own and checks that a tools workshop leaves nothing in it.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_tools)
ursatz_test(workshop_tools SOURCES test_tools.c LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_TEST_COMPOSER="${CMAKE_SOURCE_DIR}/composer/tests"
                URSATZ_TEST_SEED_CORPUS="${CMAKE_BINARY_DIR}/tests/corpora/sonatinas-analyzed"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_tools)
add_dependencies(test_workshop_tools seed_corpora)

# The Analyzer's tools (P64 B2): analyze, corpus and style-profile on a tools workshop, one small piece, compared with the Analyzer's goldens.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_analyzer_tools)
ursatz_test(workshop_analyzer_tools SOURCES test_analyzer_tools.c LIBS ursatz_workshop ursatz_test_support
    DEFINITIONS URSATZ_FIXTURES="${URSATZ_FIXTURES_DIR}"
                URSATZ_SNAPSHOTS="${CMAKE_SOURCE_DIR}/analyzer/tests/snapshots"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_analyzer_tools)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_metadata)
ursatz_test(piece_metadata LIBS ursatz_workshop_internal ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_metadata)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/id_service)
ursatz_test(id_service LIBS ursatz_workshop_internal ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/id_service)

# The migration of the GUI's data folder to layout 1, its refusals and the data check's cases (P63 B7). The test copies the version 0
# fixture into folders of its own working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_migrate)
ursatz_test(workshop_migrate SOURCES test_migrate.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_DATA_V0="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/data-v0"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_migrate)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/job_queue)
ursatz_test(job_queue LIBS ursatz_workshop_internal ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_THEME_REQUEST="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/composer/theme-request.json"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/job_queue)
# The test cancels a theme compose made from a request fixture; no server is started.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)
ursatz_test(material_service LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_STORE_DIR="${CMAKE_CURRENT_BINARY_DIR}/material_service"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)
# The test creates its own materials_<pid>/ folder in the build tree (URSATZ_GUI_TEST_STORE_DIR), works in it as its data folder, and removes it.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
ursatz_test(subject_service LIBS ursatz_workshop_internal ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
# The test creates its own projects/, subjects_<pid>/ and library/pieces/ in its working directory.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/link_service)
ursatz_test(link_service LIBS ursatz_workshop_internal ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/link_service)
# The test creates pieces/, collections/, materials/, subjects/ and links/ in its own working directory and reads the
# link fixtures.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_lock)
ursatz_test(piece_lock LIBS ursatz_workshop_internal ursatz_test_http
    DEFINITIONS URSATZ_GUI_TEST_SOURCE="${URSATZ_FIXTURES_DIR}/musicxml/chords_backup.musicxml"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/piece_lock)
# The test makes pieces/ in its own working directory and uploads this MusicXML file as the piece.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stale_analysis)
ursatz_test(stale_analysis LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stale_analysis)
# The test creates pieces/, styles/, collections/, materials/, subjects/ and links/ in its own working directory and reads
# the link fixtures.


file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_jobs)
ursatz_test(workshop_jobs SOURCES test_jobs.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_jobs)
# The test creates library/, settings/, cache/ and the stores in its own working directory, reads the link fixtures, and cancels link runs
# and style builds between pieces.

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_variants)
ursatz_test(workshop_variants SOURCES test_variants.c LIBS ursatz_workshop_internal ursatz_test_support
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
                     ${CMAKE_CURRENT_BINARY_DIR}/workshop_settings_commands ${CMAKE_CURRENT_BINARY_DIR}/workshop_ratings
                     ${CMAKE_CURRENT_BINARY_DIR}/workshop_critiques
        LABELS schema
        PROPERTIES FIXTURES_REQUIRED "migrated_routes_files;settings_commands_files;ratings_files;critiques_files")
endif()

# Recovery after a stop in the middle of a change (P63 B8): hand-made leftovers of every kind, recovered by workshop_open from the journal.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_recovery)
ursatz_test(workshop_recovery SOURCES test_recovery.c LIBS ursatz_workshop_internal WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_recovery)

# A stop at every step of every kind of change, in a forked child, and what the folder holds after the open recovers it (P63 B8). Each case copies one base
# folder, so the test makes its own folders in its working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_crash_points)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_crash_points_smoke)
ursatz_test_executable(test_workshop_crash_points SOURCES test_crash_points.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_TEST_HOOKS URSATZ_GUI_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links")
ursatz_test(workshop_crash_points TARGET test_workshop_crash_points LABELS slow
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_crash_points)
# Slow: about 25 s at -j4 (2026-11-21) (a Library piece is analyzed and a theme composed at each stop). The smoke run (argument smoke) stops the item commands alone.
ursatz_test(workshop_crash_points_smoke TARGET test_workshop_crash_points ARGS smoke
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_crash_points_smoke)

# The layers a piece and a composed theme keep (P63 B9): Mozart 545's MIDI imported, a theme composed, a reading of another digest refused. The test makes
# its own data folder in its working directory. Kept in the fast set: about 10 s at -j4 (2026-11-21).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_layers)
ursatz_test(workshop_layers SOURCES test_layers.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_TEST_MOZART="${URSATZ_FIXTURES_DIR}/framework-v1-corpus/Mozart 545.mid"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_layers)

# The query and command API (P63 B11): every command through workshop_command with its effect and its journal record, every query
# against what its route answers, and the version a command was made against. Each runs in its own working directory, which it clears.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_commands)
ursatz_test(workshop_commands SOURCES test_commands.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_commands)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_queries)
ursatz_test(workshop_queries SOURCES test_queries.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_queries)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_base_version)
ursatz_test(workshop_base_version SOURCES test_base_version.c LIBS ursatz_workshop_internal ursatz_test_support
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_base_version)

# The workshop's events (P63 B13): what a command and a job publish, in order, and the reset of a subscriber that did not read.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_events)
ursatz_test(workshop_events SOURCES test_events.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_TEST_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_events)

# The resolver of a stored request (P63 B15): references of each kind, the style of a brief, the refusals at their paths.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_request_resolve)
ursatz_test(workshop_request_resolve SOURCES test_request_resolve.c LIBS ursatz_workshop_internal ursatz_test_support
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_request_resolve)

# The migration of theme and piece bodies to stored requests (P63 B15): every recorded body (fixtures/requests) resolves to the request it
# composed with, and a layout 1 folder migrates in place to layout 2.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_body_migration)
ursatz_test(workshop_body_migration SOURCES test_body_migration.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS URSATZ_WORKSHOP_DATA_V0="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/data-v0"
                URSATZ_WORKSHOP_REQUESTS="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/requests"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_body_migration)

# Ursatz Text from the workshop (P65 B3): the tool text on a tools workshop, and item.view and library.view on a migrated copy of the layout 0 fixture
# folder. Each runs in its own working directory, which it clears.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_text)
ursatz_test(workshop_text SOURCES test_text.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS TEXT_FIXTURES="${CMAKE_SOURCE_DIR}/library/text/tests/fixtures"
                TEXT_SNAPSHOTS="${CMAKE_SOURCE_DIR}/library/text/tests/snapshots"
                TEXT_FILL_PLAN="${CMAKE_SOURCE_DIR}/composer/tests/snapshots/fill.plan.json"
                TEXT_DATA_V0="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/data-v0"
                TEXT_LINK_FIXTURES="${URSATZ_FIXTURES_DIR}/links"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_text)

# Ursatz Text over generated layers (P65 B3, slow): the layers of a composed piece of every form but the game loop and of five analyzed pieces
# read with their codecs, printed and read back. Slow: about 134 s at -j4 (2026-11-21); its smoke runs every PR.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_text_generated)
ursatz_test(workshop_text_generated SOURCES test_text_generated.c LIBS ursatz_workshop_internal ursatz_test_support LABELS slow
    DEFINITIONS TEXT_FORMS="${CMAKE_SOURCE_DIR}/composer/forms"
                TEXT_LOOP_REQUEST="${CMAKE_SOURCE_DIR}/composer/tests/fixtures/piece/structure-only.json"
                TEXT_CORPUS="${URSATZ_FIXTURES_DIR}/framework-v1-corpus"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_text_generated)
# The smoke: the form binary, the loop request and Bach 114 (nine layers).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_text_generated_smoke)
ursatz_test(workshop_text_generated_smoke TARGET test_workshop_text_generated ARGS smoke
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_text_generated_smoke)

# The commands as schemas (P65 B5): the table and the schemas agree, the check of a command's params at its path, the actor in the version envelope.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_command_schemas)
ursatz_test(workshop_command_schemas SOURCES test_command_schemas.c LIBS ursatz_workshop_internal ursatz_test_support
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_command_schemas)

# workshop/docs/command-reference.md is what the tool commands generates; the sheet stays short and every command has an example. The page is
# written with URSATZ_UPDATE_GOLDENS=1.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/command_reference)
ursatz_test(command_reference SOURCES test_command_reference.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS TEXT_COMMAND_REFERENCE="${PROJECT_SOURCE_DIR}/workshop/docs/command-reference.md"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/command_reference LABELS golden)

# Each workshop/commands/*.json against its meta-schema. Needs python3 with jsonschema (cmake/UrsatzTools.cmake).
if(URSATZ_JSONSCHEMA_PYTHON)
    file(GLOB URSATZ_COMMAND_FILES ${PROJECT_SOURCE_DIR}/workshop/commands/*.json)
    ursatz_test(command_schemas_validate COMMAND ${URSATZ_JSONSCHEMA_PYTHON} ${PROJECT_SOURCE_DIR}/tests/support/validate_schema.py
                     ${PROJECT_SOURCE_DIR}/workshop/schema/ursatz-commands.v1.schema.json ${URSATZ_COMMAND_FILES}
        LABELS schema)
endif()

# Commands in Ursatz Text (P65 B6): the tool commands-parse on a tools workshop, every rule with its line and message.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_command_text)
ursatz_test(workshop_command_text SOURCES test_command_text.c LIBS ursatz_workshop ursatz_test_support
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_command_text)

# The section commands (P65 B7): the pure edits of a piece request on the game loop fixture, then the commands on a migrated copy of the layout 0
# fixture folder (a version made and journaled, a stale base, the schema's limits, the Composer's refusal at its path, an overlay on a form piece).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_section_commands)
ursatz_test(workshop_section_commands SOURCES test_section_commands.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS TEXT_LOOP_REQUEST="${CMAKE_SOURCE_DIR}/composer/tests/fixtures/piece/structure-only.json"
                TEXT_DATA_V0="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/data-v0"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_section_commands)

# Restore a version (P65 B8): the next version a copy of an older one, with its files, an envelope that says restored_from, and the refusals.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_restore)
ursatz_test(workshop_restore SOURCES test_restore.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS TEXT_DATA_V0="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/data-v0"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_restore)

# The settings resolver (P66 B4): scope files written by hand, the order and sources of the scopes, the switches, the profile chain and every override action.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_settings_resolve)
ursatz_test(workshop_settings_resolve SOURCES test_settings_resolve.c LIBS ursatz_workshop_internal ursatz_test_support VALGRIND
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_settings_resolve)

# The settings commands (P66 B4): every command and refusal through the API, the queries, a taste profile that changes only by an import that says why. Its folder is
# one of workshop_formats' (the files the commands leave validate).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_settings_commands)
ursatz_test(workshop_settings_commands SOURCES test_settings_commands.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS SETTINGS_TASTE_NONE="${URSATZ_FIXTURES_DIR}/settings/taste-none.json"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_settings_commands
    PROPERTIES FIXTURES_SETUP settings_commands_files)

# Composing with settings (P66 B5): a theme with nothing set, with the taste switched off, with an item override and explored, the random motifs query and a piece that takes no
# settings. Composes several themes: slow if it passes a minute.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_compose_settings)
ursatz_test(workshop_compose_settings SOURCES test_compose_settings.c LIBS ursatz_workshop_internal ursatz_test_support
    DEFINITIONS SETTINGS_TASTE_NONE="${URSATZ_FIXTURES_DIR}/settings/taste-none.json"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_compose_settings)

# The critic's judgments in the data folder (P68 B6, R-723): a composed theme critiqued, a verdict, the refusals, the settings param and a theme
# composed with critic.rank on. Its folder is one of workshop_formats' (the file the commands leave validates).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_critiques)
ursatz_test(workshop_critiques SOURCES test_critiques.c LIBS ursatz_workshop_internal ursatz_test_support
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_critiques
    PROPERTIES FIXTURES_SETUP critiques_files)

# Listening sessions and ratings (P66 B6): a session with two batches, ratings and a pick, keep as default citing them, the refusals, ratings of an item version.
# Its folder is one of workshop_formats' (the files the commands leave validate).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_ratings)
ursatz_test(workshop_ratings SOURCES test_ratings.c LIBS ursatz_workshop_internal ursatz_test_support
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/workshop_ratings
    PROPERTIES FIXTURES_SETUP ratings_files)

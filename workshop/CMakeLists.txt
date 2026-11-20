# The workshop (R-641): the process that holds the data folder, its services, the job queue, the history and the events, above the
# two apps and below the GUI. Sources include <ursatz_workshop/...> and the headers beside them.
add_library(ursatz_workshop_engine STATIC
    src/base/command_context.c
    src/base/events_hub.c
    src/base/http_status.c
    src/base/knowledge.c
    src/base/last_error.c
    src/base/names.c
    src/items/collections.c
    src/items/data_check.c
    src/items/formats.c
    src/items/ids.c
    src/items/layers.c
    src/items/journal.c
    src/items/layout.c
    src/items/migrate.c
    src/items/recover.c
    src/items/txn.c
    src/items/library_pieces.c
    src/items/library_metadata.c
    src/items/store.c
    src/items/references.c
    src/items/body_rules.c
    src/items/body_migrate.c
    src/items/body_requests.c
    src/items/op_checks.c
    src/items/version_files.c
    src/items/piece_lock.c
    src/items/subjects.c
    src/analysis/analysis.c
    src/analysis/analysis_jobs.c
    src/analysis/import_cache.c
    src/analysis/links.c
    src/analysis/passages.c
    src/analysis/styles.c
    src/composition/compose_common.c
    src/composition/compose_jobs.c
    src/composition/composer_client.c
    src/composition/composer_tool.c
    src/composition/motif_import.c
    src/composition/motifs.c
    src/composition/pieces.c
    src/composition/request_resolve.c
    src/composition/themes.c
    src/composition/variants.c
    src/rendering/export.c
    src/rendering/playback.c
    src/rendering/render_cache.c
    src/jobs/job_queue.c
    src/coordinator/api_analysis.c
    src/coordinator/api_collections.c
    src/coordinator/api_commands_items.c
    src/coordinator/api_commands_library.c
    src/coordinator/api_composer.c
    src/coordinator/api_items.c
    src/coordinator/api_jobs.c
    src/coordinator/api_library.c
    src/coordinator/data_lock.c
    src/coordinator/dispatch.c
    src/coordinator/events.c
    src/coordinator/settings.c
    src/coordinator/tools.c
    src/coordinator/workshop.c
)
ursatz_public_headers(ursatz_workshop_engine
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/coordinator
    HEADERS
        ursatz_workshop/workshop_api.h
)
# The files of src/ include one another as "<folder>/<name>.h"; only the API header (include/) is visible to the programs that use the
# workshop. The workshop library is the engine archive merged into one object that exports only the names of exports.txt (R-640,
# cmake/UrsatzExports.cmake); the tests of the internals link ursatz_workshop_internal, the engine with src/ on the include path. Never link
# ursatz_workshop and ursatz_workshop_internal into one executable.
target_include_directories(ursatz_workshop_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ursatz_workshop_engine
    PUBLIC ursatz_common ursatz_composer ursatz_analyzer ursatz_theory ursatz_realization ursatz_reading cjson
)
target_compile_options(ursatz_workshop_engine PRIVATE -Wall -Wextra -Wpedantic)
if(URSATZ_TESTS)
    # The test build of the engine can stop a commit at each of its steps (items/txn.h); without a point set the hooks do nothing.
    target_compile_definitions(ursatz_workshop_engine PRIVATE URSATZ_WORKSHOP_TEST_HOOKS)
endif()
ursatz_localize_exports(ursatz_workshop ENGINE ursatz_workshop_engine KEEP ${CMAKE_CURRENT_SOURCE_DIR}/src/exports.txt)
add_library(ursatz_workshop_internal INTERFACE)
target_include_directories(ursatz_workshop_internal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ursatz_workshop_internal INTERFACE ursatz_workshop_engine)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

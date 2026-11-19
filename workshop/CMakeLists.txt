# The workshop (R-641): the process that holds the data folder, its services, the job queue, the history and the events, above the
# two apps and below the GUI. Sources include <ursatz_workshop/...> and the headers beside them.
add_library(ursatz_workshop STATIC
    src/base/http_status.c
    src/base/knowledge.c
    src/base/last_error.c
    src/items/collections.c
    src/items/data_check.c
    src/items/ids.c
    src/items/library_pieces.c
    src/items/library_metadata.c
    src/items/store.c
    src/items/references.c
    src/items/body_rules.c
    src/items/body_migrate.c
    src/items/op_checks.c
    src/items/version_files.c
    src/items/piece_lock.c
    src/items/subjects.c
    src/analysis/analysis.c
    src/analysis/import_cache.c
    src/analysis/links.c
    src/analysis/styles.c
    src/composition/compose_common.c
    src/composition/compose_jobs.c
    src/composition/composer_client.c
    src/composition/motif_import.c
    src/composition/motifs.c
    src/composition/pieces.c
    src/composition/themes.c
    src/composition/variants.c
    src/rendering/export.c
    src/rendering/playback.c
    src/rendering/render_cache.c
    src/jobs/job_queue.c
    src/coordinator/data_lock.c
    src/coordinator/settings.c
    src/coordinator/workshop.c
)
ursatz_public_headers(ursatz_workshop
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/coordinator
    HEADERS
        ursatz_workshop/workshop_api.h
)
# workshop_internal.h, the one header the GUI routes include for the services, stays visible to the GUI backend until B11 gives it the
# query and command API (and the files of src/ include one another as "<folder>/<name>.h").
target_include_directories(ursatz_workshop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ursatz_workshop
    PUBLIC ursatz_common ursatz_composer ursatz_analyzer ursatz_theory cjson
)
target_compile_options(ursatz_workshop PRIVATE -Wall -Wextra -Wpedantic)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

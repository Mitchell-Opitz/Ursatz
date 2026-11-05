# Everything but main.c, so the tests link the same code the server runs.
add_library(ursatz_gui_core STATIC
  src/server.c
  src/util/fs_util.c
  src/routes/route_util.c
  src/service/persistence_service.c
  src/service/analysis_service.c
  src/service/import_cache.c
  src/service/piece_metadata.c
  src/service/id_service.c
  src/service/collection_service.c
  src/service/style_service.c
  src/service/composer_client.c
  src/service/job_queue.c
  src/service/compose_jobs.c
  src/service/material_service.c
  src/service/motif_service.c
  src/service/compose_common.c
  src/service/piece_lock.c
  src/service/theme_service.c
  src/service/piece_service.c
  src/service/playback_service.c
  src/service/subject_service.c
  src/service/link_service.c
  src/routes/health.c
  src/routes/analyzer.c
  src/routes/corpus.c
  src/routes/composer.c
  src/routes/styles.c
  src/routes/materials.c
  src/routes/links.c
  src/routes/jobs.c
)

target_include_directories(ursatz_gui_core PUBLIC
  include
  src
)

target_link_libraries(ursatz_gui_core PUBLIC
  ursatz_composer
  ursatz_analyzer
  cjson
  ursatz
  civetweb-c-library
)

if(WIN32)
  target_link_libraries(ursatz_gui_core PUBLIC ws2_32)
endif()

target_compile_definitions(ursatz_gui_core PUBLIC
  URSATZ_GUI_FRONTEND_DIR="${CMAKE_SOURCE_DIR}/frontend/public"
)

add_executable(ursatz-gui-backend src/main.c)

target_link_libraries(ursatz-gui-backend PRIVATE ursatz_gui_core)


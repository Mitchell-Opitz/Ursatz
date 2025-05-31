add_executable(ursatz-gui-backend
  src/main.c
  src/server.c
  src/service/persistence_service.c
  src/service/analysis_service.c
  src/service/piece_metadata.c
  src/service/id_service.c
  src/service/collection_service.c
  src/service/style_service.c
  src/service/composer_service.c
  src/service/composition_service.c
  src/routes/health.c
  src/routes/analyzer.c
  src/routes/corpus.c
  src/routes/composer.c
  src/routes/styles.c
)

target_include_directories(ursatz-gui-backend PRIVATE
  include
  src
)

target_link_libraries(ursatz-gui-backend PRIVATE
  ursatz_analyzer
  cjson
  ursatz
  civetweb-c-library
)

target_compile_definitions(ursatz-gui-backend PRIVATE
  URSATZ_GUI_FRONTEND_DIR="${CMAKE_SOURCE_DIR}/frontend/public"
)

# The Composer command the backend runs (composer_service.c).
add_dependencies(ursatz-gui-backend ursatz_composer_cli)

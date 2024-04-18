add_executable(ursatz-gui-backend
  src/main.c
  src/server.c
  src/service/persistence_service.c
  src/service/analysis_service.c
  src/service/corpus_service.c
  src/routes/health.c
  src/routes/analyzer.c
  src/routes/corpus.c
)

target_include_directories(ursatz-gui-backend PRIVATE
  include
  src
)

target_link_libraries(ursatz-gui-backend PRIVATE
  ursatz
  common_practice_minimal
  civetweb-c-library
)

target_compile_definitions(ursatz-gui-backend PRIVATE
  URSATZ_GUI_FRONTEND_DIR="${CMAKE_SOURCE_DIR}/frontend/public"
)

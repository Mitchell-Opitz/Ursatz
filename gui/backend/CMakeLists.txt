# Everything but main.c, so the tests link the same code the server runs.
add_library(ursatz_gui_core STATIC
  src/server.c
  src/routes/route_util.c
  src/routes/analyzer.c
  src/routes/corpus.c
  src/routes/composer.c
  src/routes/styles.c
  src/routes/materials.c
  src/routes/links.c
  src/routes/jobs.c
)

target_include_directories(ursatz_gui_core PUBLIC
  src
)

target_link_libraries(ursatz_gui_core PUBLIC
  ursatz_workshop
  ursatz_common
  cjson
  civetweb-c-library
)

if(WIN32)
  target_link_libraries(ursatz_gui_core PUBLIC ws2_32)
endif()

target_compile_definitions(ursatz_gui_core PUBLIC
  URSATZ_GUI_FRONTEND_DIR="${PROJECT_SOURCE_DIR}/frontend/public"
)

add_executable(ursatz-gui-backend src/main.c)

target_link_libraries(ursatz-gui-backend PRIVATE ursatz_gui_core)


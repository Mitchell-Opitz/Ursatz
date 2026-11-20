# Everything but main.c, so the tests link the same code the server runs. The GUI links the workshop library, which exports only its API;
# the route tests that call the workshop's internals link the second copy of the same sources, built on the engine (never both).
set(URSATZ_GUI_CORE_SOURCES
  src/server.c
  src/routes/route_table.c
  src/routes/route_util.c
  src/routes/library_routes.c
  src/routes/collections_routes.c
  src/routes/composer_routes.c
  src/routes/events_routes.c
  src/routes/styles_routes.c
  src/routes/materials_routes.c
  src/routes/motifs_routes.c
  src/routes/subjects_routes.c
  src/routes/links_routes.c
  src/routes/jobs_routes.c
  src/routes/workshop_routes.c
)

foreach(core ursatz_gui_core ursatz_gui_core_internal)
  add_library(${core} STATIC ${URSATZ_GUI_CORE_SOURCES})
  target_include_directories(${core} PUBLIC src)
  target_link_libraries(${core} PUBLIC ursatz_common cjson civetweb-c-library)
  if(WIN32)
    target_link_libraries(${core} PUBLIC ws2_32)
  endif()
  target_compile_definitions(${core} PUBLIC URSATZ_GUI_FRONTEND_DIR="${PROJECT_SOURCE_DIR}/frontend/public")
endforeach()
target_link_libraries(ursatz_gui_core PUBLIC ursatz_workshop)
target_link_libraries(ursatz_gui_core_internal PUBLIC ursatz_workshop_internal)

add_executable(ursatz-gui-backend src/main.c)

target_link_libraries(ursatz-gui-backend PRIVATE ursatz_gui_core)


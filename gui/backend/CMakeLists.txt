add_executable(ursatz-gui-backend
  src/main.c
  src/server.c
  src/service/persistence_service.c
  src/routes/health.c
)

target_include_directories(ursatz-gui-backend PRIVATE
  include
  src
)

target_link_libraries(ursatz-gui-backend PRIVATE
  ursatz
  civetweb-c-library
)

target_compile_definitions(ursatz-gui-backend PRIVATE
  URSATZ_GUI_FRONTEND_DIR="${CMAKE_SOURCE_DIR}/frontend/public"
)

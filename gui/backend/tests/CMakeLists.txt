add_executable(test_health_route test_health_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
)

target_include_directories(test_health_route PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/include
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_health_route PRIVATE
  ursatz
  civetweb-c-library
)

add_test(NAME health_route COMMAND test_health_route)

add_executable(test_health_route test_health_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/analysis_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
)

target_include_directories(test_health_route PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/include
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_health_route PRIVATE
  ursatz
  common_practice_minimal
  civetweb-c-library
)

if(WIN32)
  target_link_libraries(test_health_route PRIVATE ws2_32)
endif()

add_test(NAME health_route COMMAND test_health_route)

add_executable(test_analyzer_route test_analyzer_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/analysis_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
)

target_include_directories(test_analyzer_route PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/include
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_analyzer_route PRIVATE
  ursatz
  common_practice_minimal
  civetweb-c-library
)

if(WIN32)
  target_link_libraries(test_analyzer_route PRIVATE ws2_32)
endif()

add_test(NAME analyzer_route COMMAND test_analyzer_route)

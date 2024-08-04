add_executable(test_health_route test_health_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/analysis_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/corpus_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
)

target_include_directories(test_health_route PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/include
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_health_route PRIVATE
  ursatz_analyzer
  cjson
  ursatz
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
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/corpus_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
)

target_include_directories(test_analyzer_route PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/include
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_analyzer_route PRIVATE
  ursatz_analyzer
  cjson
  ursatz
  civetweb-c-library
)

if(WIN32)
  target_link_libraries(test_analyzer_route PRIVATE ws2_32)
endif()

add_test(NAME analyzer_route COMMAND test_analyzer_route)

add_executable(test_corpus_route test_corpus_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/analysis_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/corpus_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
)

target_include_directories(test_corpus_route PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/include
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_corpus_route PRIVATE
  ursatz_analyzer
  cjson
  ursatz
  civetweb-c-library
)

if(WIN32)
  target_link_libraries(test_corpus_route PRIVATE ws2_32)
endif()

add_test(NAME corpus_route COMMAND test_corpus_route)

add_executable(test_id_service test_id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
)

target_include_directories(test_id_service PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/src
)

add_test(NAME id_service COMMAND test_id_service)

add_executable(test_health_route test_health_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/analysis_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/piece_metadata.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/collection_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/style_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composer_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composition_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
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
  ${CMAKE_SOURCE_DIR}/backend/src/service/piece_metadata.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/collection_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/style_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composer_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composition_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
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

# The test compares the stored analysis with what the Analyzer CLI writes for the same file, in its own
# working directory (pieces/ and cli/ are created there).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/analyzer_route)
add_dependencies(test_analyzer_route ursatz-analyzer)
target_compile_definitions(test_analyzer_route PRIVATE
  URSATZ_GUI_TEST_URSATZ_DIR="${CMAKE_SOURCE_DIR}/external/ursatz"
  URSATZ_GUI_TEST_ANALYZER_CLI="$<TARGET_FILE:ursatz-analyzer>"
)

add_test(NAME analyzer_route COMMAND test_analyzer_route
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/analyzer_route)

add_executable(test_corpus_route test_corpus_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/analysis_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/piece_metadata.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/collection_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/style_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composer_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composition_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
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

# The test creates collections/, pieces/ and data/ in its own working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_route)
add_test(NAME corpus_route COMMAND test_corpus_route
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_route)

add_executable(test_piece_metadata test_piece_metadata.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/piece_metadata.c
)

target_include_directories(test_piece_metadata PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/src
)

# piece_metadata.h includes analysis_service.h, which needs Ursatz's headers.
target_link_libraries(test_piece_metadata PRIVATE ursatz)

add_test(NAME piece_metadata COMMAND test_piece_metadata)

add_executable(test_id_service test_id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
)

target_include_directories(test_id_service PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/src
)

add_test(NAME id_service COMMAND test_id_service)

add_executable(test_composer_route test_composer_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/analysis_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/piece_metadata.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/collection_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/style_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composer_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composition_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
)

target_include_directories(test_composer_route PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/include
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_composer_route PRIVATE
  ursatz_analyzer
  cjson
  ursatz
  civetweb-c-library
)

# The test runs the real Composer built beside the backend, in its own working
# directory (compositions/ and the fake Composers are created there).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/composer_route)
add_dependencies(test_composer_route ursatz_composer_cli)
target_compile_definitions(test_composer_route PRIVATE
  URSATZ_GUI_TEST_COMPOSER_DIR="${CMAKE_SOURCE_DIR}/external/ursatz-composer"
)

add_test(NAME composer_route COMMAND test_composer_route
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/composer_route)

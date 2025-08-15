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
  ${CMAKE_SOURCE_DIR}/backend/src/service/material_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/subject_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/materials.c
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
  ${CMAKE_SOURCE_DIR}/backend/src/service/material_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/subject_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/materials.c
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
  ${CMAKE_SOURCE_DIR}/backend/src/service/material_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/subject_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/materials.c
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
  ${CMAKE_SOURCE_DIR}/backend/src/service/material_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/subject_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/materials.c
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
  URSATZ_GUI_TEST_URSATZ_DIR="${CMAKE_SOURCE_DIR}/external/ursatz"
)

add_test(NAME composer_route COMMAND test_composer_route
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/composer_route)

add_executable(test_styles_route test_styles_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/analysis_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/piece_metadata.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/collection_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/style_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composer_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composition_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/material_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/subject_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/materials.c
)

target_include_directories(test_styles_route PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/include
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_styles_route PRIVATE
  ursatz_analyzer
  cjson
  ursatz
  civetweb-c-library
)

if(WIN32)
  target_link_libraries(test_styles_route PRIVATE ws2_32)
endif()

# The test builds styles from the reference pieces; it creates pieces/, collections/ and styles/ in its own
# working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/styles_route)
target_compile_definitions(test_styles_route PRIVATE
  URSATZ_GUI_TEST_URSATZ_DIR="${CMAKE_SOURCE_DIR}/external/ursatz"
)

add_test(NAME styles_route COMMAND test_styles_route
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/styles_route)

add_executable(test_material_service test_material_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/material_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/subject_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/collection_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/piece_metadata.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
)

target_include_directories(test_material_service PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_material_service PRIVATE cjson ursatz)

# The test creates its own materials_<pid>/ store in its working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)
add_test(NAME material_service COMMAND test_material_service
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/material_service)

add_executable(test_subject_service test_subject_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/material_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/subject_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/collection_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/piece_metadata.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
)

target_include_directories(test_subject_service PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_subject_service PRIVATE cjson ursatz)

# The test creates its own materials_<pid>/, subjects_<pid>/ and pieces/ in its working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)
add_test(NAME subject_service COMMAND test_subject_service
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/subject_service)

add_executable(test_materials_route test_materials_route.c
  ${CMAKE_SOURCE_DIR}/backend/src/server.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/persistence_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/analysis_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/piece_metadata.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/id_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/collection_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/style_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composer_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/composition_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/material_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/service/subject_service.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/health.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/analyzer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/corpus.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/composer.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/styles.c
  ${CMAKE_SOURCE_DIR}/backend/src/routes/materials.c
)

target_include_directories(test_materials_route PRIVATE
  ${CMAKE_SOURCE_DIR}/backend/include
  ${CMAKE_SOURCE_DIR}/backend/src
)

target_link_libraries(test_materials_route PRIVATE
  ursatz_analyzer
  cjson
  ursatz
  civetweb-c-library
)

if(WIN32)
  target_link_libraries(test_materials_route PRIVATE ws2_32)
endif()

# The test creates materials/, subjects/ and pieces/ in its own working directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/materials_route)
add_test(NAME materials_route COMMAND test_materials_route
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/materials_route)

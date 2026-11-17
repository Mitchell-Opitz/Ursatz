# Root tests: checks that hold for the whole tree.
ursatz_test(test_asserts_on)

# Every include follows cmake/layer-table.txt (spec/target-architecture.md 12.2).
ursatz_test(layer_table COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/check_layer_table.py ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/library/public/manifest.txt)

# Test support every part shares: checks, files, temp paths and JSON (tests/support/ursatz_test.h), and the HTTP client of
# the GUI backend tests (tests/support/test_http.h). A part's own helpers stay in its tests/support folder.
ursatz_test_library(ursatz_test_support SOURCES support/ursatz_test.c LIBS cjson
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/support)
ursatz_test_library(ursatz_test_http SOURCES support/test_http.c LIBS ursatz_test_support cjson)
if(WIN32)
    target_link_libraries(ursatz_test_http PUBLIC ws2_32)
endif()

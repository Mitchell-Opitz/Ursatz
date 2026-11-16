# Root tests: checks that hold for the whole tree.
ursatz_test(test_asserts_on)

# Every include follows cmake/layer-table.txt (spec/target-architecture.md 12.2).
ursatz_test(layer_table COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/check_layer_table.py ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/library/public/manifest.txt)

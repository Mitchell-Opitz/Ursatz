# corpus_stats_api needs real Chord construction helpers from
# common-practice-minimal (frameworks/common-practice-minimal), not just
# ursatz -- add_ursatz_test only links ursatz, so this one is wired
# manually, the same way tests/persistence/CMakeLists.txt wires
# test_persistence_chord_key.
add_executable(test_corpus_stats_api test_corpus_stats_api.c)
target_link_libraries(test_corpus_stats_api PRIVATE common_practice_minimal)
add_test(NAME corpus_stats_api COMMAND test_corpus_stats_api)

add_ursatz_architecture_test(corpus_stats)

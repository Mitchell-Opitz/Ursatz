add_executable(test_smoke test_smoke.c)
target_link_libraries(test_smoke PRIVATE ursatz_composer)
add_test(NAME test_smoke COMMAND test_smoke)
target_compile_options(test_smoke PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_piece_plan_sentence test_piece_plan_sentence.c)
target_link_libraries(test_piece_plan_sentence PRIVATE ursatz_composer)
target_compile_options(test_piece_plan_sentence PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME test_piece_plan_sentence COMMAND test_piece_plan_sentence)

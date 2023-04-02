add_executable(test_smoke test_smoke.c)
target_link_libraries(test_smoke PRIVATE ursatz_composer)
add_test(NAME test_smoke COMMAND test_smoke)
target_compile_options(test_smoke PRIVATE -Wall -Wextra -Wpedantic)

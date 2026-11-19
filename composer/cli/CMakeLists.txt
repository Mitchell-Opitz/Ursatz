add_executable(ursatz-composer main.c cli_context.c cli_options.c progression.c motive.c develop.c grow.c grow_text.c theme.c piece.c piece_text.c counterpoint.c plan.c fill.c regenerate.c variations.c listen.c workflow.c variants_cmd.c random_motive.c motive_extras.c motive_input.c motive_text.c recognize.c)
# The CLI orchestrates the two apps until P64's one CLI (R-619): it finds the corpus passages with the Analyzer and hands them to the Composer.
target_link_libraries(ursatz-composer PRIVATE ursatz_composer ursatz_analyzer ursatz_theory ursatz_common cjson)
target_compile_options(ursatz-composer PRIVATE -Wall -Wextra -Wpedantic)

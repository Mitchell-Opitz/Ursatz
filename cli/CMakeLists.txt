# The program ursatz (R-657): the Composer command line, moved here and renamed. It orchestrates the two apps until P64 B4 (R-619): it finds the
# corpus passages with the Analyzer and hands them to the Composer.
set(URSATZ_CLI_SOURCES
    main.c cli_context.c cli_options.c progression.c motive.c develop.c grow.c grow_text.c theme.c piece.c piece_text.c counterpoint.c plan.c
    fill.c regenerate.c variations.c listen.c workflow.c variants_cmd.c random_motive.c motive_extras.c motive_input.c motive_text.c recognize.c)
list(TRANSFORM URSATZ_CLI_SOURCES PREPEND src/)
add_executable(ursatz ${URSATZ_CLI_SOURCES})
target_link_libraries(ursatz PRIVATE ursatz_composer_engine ursatz_analyzer ursatz_theory ursatz_common cjson)
target_compile_options(ursatz PRIVATE -Wall -Wextra -Wpedantic)

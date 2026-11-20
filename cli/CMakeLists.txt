# The program ursatz (R-657): the Composer command line, moved here and renamed. It calls the workshop and nothing else (R-656): every Composer
# call is the tool composer, the corpus seeding the tool corpus-passages and the pitch names the tool pitch-names.
set(URSATZ_CLI_SOURCES
    main.c cli_tools.c cli_options.c progression.c motive.c develop.c grow.c grow_text.c theme.c piece.c piece_text.c counterpoint.c plan.c
    fill.c regenerate.c variations.c listen.c workflow.c variants_cmd.c random_motive.c motive_extras.c motive_input.c motive_text.c recognize.c)
list(TRANSFORM URSATZ_CLI_SOURCES PREPEND src/)
add_executable(ursatz ${URSATZ_CLI_SOURCES})
target_link_libraries(ursatz PRIVATE ursatz_workshop ursatz_common cjson)
target_compile_options(ursatz PRIVATE -Wall -Wextra -Wpedantic)

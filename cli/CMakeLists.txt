# The program ursatz (R-657): the Composer command line, moved here and renamed. It calls the workshop and nothing else (R-656): every Composer
# call is the tool composer, the corpus seeding the tool corpus-passages and the pitch names the tool pitch-names.
# The commands and the tables are the static library ursatz_cli, so the tests link the flag table and the command table; the program is main.c over it.
set(URSATZ_CLI_SOURCES
    commands.c flags.c analyze.c corpus.c session.c remote.c status.c query.c address.c show.c apply.c render.c cli_tools.c cli_options.c progression.c motive.c develop.c grow.c grow_text.c theme.c piece.c piece_text.c counterpoint.c
    plan.c fill.c regenerate.c variations.c listen.c workflow.c variants_cmd.c random_motive.c motive_extras.c motive_input.c motive_text.c
    recognize.c)
list(TRANSFORM URSATZ_CLI_SOURCES PREPEND src/)
add_library(ursatz_cli STATIC ${URSATZ_CLI_SOURCES})
target_include_directories(ursatz_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ursatz_cli PUBLIC ursatz_workshop ursatz_common cjson civetweb-c-library)
target_compile_options(ursatz_cli PRIVATE -Wall -Wextra -Wpedantic)
add_executable(ursatz src/main.c)
target_link_libraries(ursatz PRIVATE ursatz_cli)
target_compile_options(ursatz PRIVATE -Wall -Wextra -Wpedantic)
if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

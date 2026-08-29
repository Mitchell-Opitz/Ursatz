add_executable(ursatz-composer main.c cli_options.c progression.c motive.c develop.c grow.c theme.c piece.c counterpoint.c plan.c fill.c regenerate.c variations.c variants_cmd.c random_motive.c motive_extras.c motive_input.c recognize.c)
target_link_libraries(ursatz-composer PRIVATE ursatz_composer cjson)
target_compile_options(ursatz-composer PRIVATE -Wall -Wextra -Wpedantic)

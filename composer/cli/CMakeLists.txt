add_executable(ursatz-composer main.c cli_options.c progression.c motive.c develop.c grow.c counterpoint.c plan.c fill.c regenerate.c variations.c variants_cmd.c)
target_link_libraries(ursatz-composer PRIVATE ursatz_composer cjson)
target_compile_options(ursatz-composer PRIVATE -Wall -Wextra -Wpedantic)

add_executable(ursatz-composer main.c cli_options.c progression.c motive.c develop.c grow.c grow_text.c theme.c request_read.c piece.c counterpoint.c plan.c fill.c regenerate.c variations.c variants_cmd.c random_motive.c motive_extras.c motive_input.c motive_text.c recognize.c)
target_link_libraries(ursatz-composer PRIVATE ursatz_composer cjson)
# The private helpers of the library (src/util), which the CLI shares.
target_include_directories(ursatz-composer PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_options(ursatz-composer PRIVATE -Wall -Wextra -Wpedantic)

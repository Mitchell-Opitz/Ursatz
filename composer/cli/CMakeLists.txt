add_executable(ursatz-composer main.c)
target_link_libraries(ursatz-composer PRIVATE ursatz_composer)
target_compile_options(ursatz-composer PRIVATE -Wall -Wextra -Wpedantic)

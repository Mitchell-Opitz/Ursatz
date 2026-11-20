# The tests of the cli part (P64 B5): the flag table and its parser, and the help printed from the tables.
ursatz_test(cli_flags SOURCES test_flags.c LIBS ursatz_cli ursatz_workshop ursatz_common cjson)
ursatz_test(cli_help COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_help.cmake)

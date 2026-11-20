# The tests of the cli part (P64 B5): the flag table and its parser, and the help printed from the tables.
ursatz_test(cli_flags SOURCES test_flags.c LIBS ursatz_cli ursatz_workshop ursatz_common cjson)
ursatz_test(cli_help COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_help.cmake)

# Connect or run (P64 B7): the data folder opened by the command, held by another process, served by the GUI backend. Run in a working directory of
# its own; the programs are run as processes.
if(URSATZ_GUI)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_connect)
    ursatz_test(cli_connect SOURCES test_connect.c LIBS ursatz_workshop ursatz_test_http ursatz_common cjson
        DEFINITIONS URSATZ_BIN="$<TARGET_FILE:ursatz>" URSATZ_BACKEND="$<TARGET_FILE:ursatz-gui-backend>"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_connect)
    add_dependencies(test_cli_connect ursatz ursatz-gui-backend)
endif()

# The data folder subcommands (P64 B8): migrate, query, show, compose, history, render and apply on a copy of the layout 0 fixture folder.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_data_commands)
ursatz_test(cli_data_commands COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -DDATA_V0=${CMAKE_SOURCE_DIR}/workshop/tests/fixtures/data-v0
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_data_commands/work -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_data_commands.cmake)

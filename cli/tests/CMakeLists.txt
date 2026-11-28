# The tests of the cli part (P64 B5): the flag table and its parser, and the help printed from the tables.
ursatz_test(cli_flags SOURCES test_flags.c LIBS ursatz_cli ursatz_workshop ursatz_common cjson)
ursatz_test(cli_help COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -DCOMMANDS_SOURCE=${CMAKE_SOURCE_DIR}/cli/src/commands.c -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_help.cmake)

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

# ursatz critique and listen --plans (P68 B4, R-721): candidate files judged as one set, in text and JSON, and a rated set judged pair by pair.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_knowledge)
ursatz_test(cli_knowledge COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_knowledge/work
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_knowledge.cmake)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_critique)
ursatz_test(cli_critique COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_critique/work
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_critique.cmake TIMEOUT 300)

# Ursatz Text on the command line (P65 B6): show with its views and refusals, apply with text commands, the generated help; and the flag reference page.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_text)
ursatz_test(cli_text COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -DDATA_V0=${CMAKE_SOURCE_DIR}/workshop/tests/fixtures/data-v0
    -DPLAN=${CMAKE_SOURCE_DIR}/library/text/tests/fixtures/loop.plan.json -DBLUEPRINT=${CMAKE_SOURCE_DIR}/composer/tests/fixtures/theme/blueprint-sentence
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_text/work
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_text.cmake)
ursatz_test(cli_flags_doc COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -DPAGE=${CMAKE_SOURCE_DIR}/cli/docs/flags.md
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_flags_doc.cmake)

# "Add a 4-bar phrase between A and B" with ursatz apply alone (P65 B7): two section inserts and a compose from a text file, then the summary.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_text_apply)
ursatz_test(cli_text_apply COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -DDATA_V0=${CMAKE_SOURCE_DIR}/workshop/tests/fixtures/data-v0
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_text_apply/work -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_text_apply.cmake)

# A listening round (P66 B7, R-674): the none profile imported, a session, two batches of listen --session, rate with a pick and keep as default citing it.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_listening_round)
ursatz_test(cli_listening_round COMMAND ${CMAKE_COMMAND} -DURSATZ=$<TARGET_FILE:ursatz> -DFIXTURES=${URSATZ_FIXTURES_DIR}/settings
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_listening_round/work -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_listening_round.cmake TIMEOUT 300)

# The page cli/docs/flags.md is what `ursatz help flags --format markdown` prints (P65 B6).
# Run with: cmake -DURSATZ=<ursatz> -DPAGE=<cli/docs/flags.md> -P cli_flags_doc.cmake

if(NOT URSATZ OR NOT PAGE)
    message(FATAL_ERROR "give -DURSATZ and -DPAGE")
endif()
execute_process(COMMAND "${URSATZ}" help flags --format markdown RESULT_VARIABLE status OUTPUT_VARIABLE printed ERROR_VARIABLE err)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "ursatz help flags --format markdown: exit ${status}: ${err}")
endif()
file(READ "${PAGE}" page)
if(NOT printed STREQUAL page)
    message(FATAL_ERROR "${PAGE} is not what `ursatz help flags --format markdown` prints: write it with `ursatz help flags --format markdown > cli/docs/flags.md`")
endif()
# the text form lists the same flags
execute_process(COMMAND "${URSATZ}" help flags RESULT_VARIABLE status OUTPUT_VARIABLE text)
if(NOT status EQUAL 0 OR NOT text MATCHES "--given-melody <file>")
    message(FATAL_ERROR "ursatz help flags: exit ${status}")
endif()
execute_process(COMMAND "${URSATZ}" help flags --format json RESULT_VARIABLE status ERROR_VARIABLE err)
if(status EQUAL 0 OR NOT err MATCHES "--format must be text or markdown")
    message(FATAL_ERROR "ursatz help flags --format json: exit ${status}, stderr: ${err}")
endif()
message(STATUS "ok")

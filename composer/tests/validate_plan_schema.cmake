# Validates the plan files against schema/plan.v2.schema.json: the CLI snapshots' plan files and every plan file the
# plan JSON test wrote back from the fixtures and snapshots (plan_written_*). Run with cmake -P and -DPYTHON,
# -DHAVE_JSONSCHEMA, -DVALIDATOR, -DSCHEMA, -DSNAPSHOT_DIR and -DWRITTEN_DIR. Prints a SKIPPED line (matched by the
# test's SKIP_REGULAR_EXPRESSION) when python3 with jsonschema was not found at configure time.

if(NOT HAVE_JSONSCHEMA)
    message("SKIPPED: python3 with the jsonschema module was not found at configure time")
    return()
endif()

file(GLOB snapshots "${SNAPSHOT_DIR}/*.plan.json")
file(GLOB written "${WRITTEN_DIR}/plan_written_*")
if(NOT snapshots OR NOT written)
    message(FATAL_ERROR "no plan files to validate (snapshots: ${SNAPSHOT_DIR}, written: ${WRITTEN_DIR})")
endif()

execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA}" ${snapshots} ${written} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "a plan file does not match the schema")
endif()
list(LENGTH snapshots snapshot_count)
list(LENGTH written written_count)
message(STATUS "${snapshot_count} snapshot plans and ${written_count} written plans: valid")

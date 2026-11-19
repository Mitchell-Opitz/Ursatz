# Validates the plan files against library/plan/schema/plan.v6.schema.json: the CLI snapshots' plan files and every plan file the
# plan JSON test wrote back from the fixtures and snapshots (plan_written_*). Run with cmake -P and -DPYTHON,
# -DVALIDATOR, -DSCHEMA, -DFRAGMENT (the section fragment of the plan group), -DSNAPSHOT_DIR and -DWRITTEN_DIR. The test is registered only when python3 with jsonschema was
# found at configure time (-DPYTHON is that python3).

file(GLOB snapshots "${SNAPSHOT_DIR}/*.plan.json")
file(GLOB written "${WRITTEN_DIR}/plan_written_*")
if(NOT snapshots OR NOT written)
    message(FATAL_ERROR "no plan files to validate (snapshots: ${SNAPSHOT_DIR}, written: ${WRITTEN_DIR})")
endif()

execute_process(COMMAND "${PYTHON}" "${VALIDATOR}" --ref "${FRAGMENT}" "${SCHEMA}" ${snapshots} ${written} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "a plan file does not match the schema")
endif()
list(LENGTH snapshots snapshot_count)
list(LENGTH written written_count)
message(STATUS "${snapshot_count} snapshot plans and ${written_count} written plans: valid")

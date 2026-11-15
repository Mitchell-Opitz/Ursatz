# Proves that snapshot_json_equal ignores member order and indentation and fails on a changed value.
# Run with: cmake -DWORK=<empty working directory> -P cli_snapshot_compare.cmake
include("${CMAKE_CURRENT_LIST_DIR}/snapshot_json.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

file(WRITE "${WORK}/expected.json" "{\"a\": 1, \"b\": [1, 2, {\"c\": \"x\", \"d\": null}], \"e\": 1.5}\n")
file(WRITE "${WORK}/reordered.json" "{\"e\": 1.5, \"b\": [1, 2, {\"d\": null, \"c\": \"x\"}], \"a\": 1}")
file(WRITE "${WORK}/indented.json" "{\n    \"a\": 1,\n    \"b\": [\n        1,\n        2,\n        {\n            \"c\": \"x\",\n            \"d\": null\n        }\n    ],\n    \"e\": 1.5\n}\n")
file(WRITE "${WORK}/value.json" "{\"a\": 2, \"b\": [1, 2, {\"c\": \"x\", \"d\": null}], \"e\": 1.5}\n")
file(WRITE "${WORK}/array-order.json" "{\"a\": 1, \"b\": [2, 1, {\"c\": \"x\", \"d\": null}], \"e\": 1.5}\n")
file(WRITE "${WORK}/missing.json" "{\"a\": 1, \"b\": [1, 2, {\"c\": \"x\"}], \"e\": 1.5}\n")
file(WRITE "${WORK}/broken.json" "{\"a\": 1,")

foreach(name reordered indented)
    snapshot_json_equal("${WORK}/${name}.json" "${WORK}/expected.json" same)
    if(NOT same)
        message(SEND_ERROR "${name}.json should equal expected.json")
    endif()
endforeach()
foreach(name value array-order missing broken)
    snapshot_json_equal("${WORK}/${name}.json" "${WORK}/expected.json" same)
    if(same)
        message(SEND_ERROR "${name}.json should differ from expected.json")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")

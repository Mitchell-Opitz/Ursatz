# The plan group (R-605, R-625): the plan, a tree of nodes with lanes and constraints over a piece's time (plan.h, plan_lanes.h,
# plan_constraint.h, plan_state.h, plan_edit.h, plan_json.h, plan_common.h), and the section description shared by forms, plans and
# piece requests: its value, vocabulary and rules, and its JSON codec over one attribute table. Sources include headers by their path
# from library/ (plan/<file>.h). A header is staged when a file outside the group includes it (the include closure, R-578);
# plan_internal.h, plan_json_internal.h and plan_constraint_kinds.h are the group's own.
add_library(ursatz_plan
    plan.c
    plan_common.c
    plan_constraint.c
    plan_constraint_kinds.c
    plan_edit.c
    plan_json.c
    plan_json_constraints.c
    plan_json_lanes.c
    plan_json_nodes.c
    plan_key_spans.c
    plan_lanes.c
    plan_state.c
    section.c
    section_json.c
)
ursatz_public_headers(ursatz_plan
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
    HEADERS
        plan/plan.h
        plan/plan_common.h
        plan/plan_constraint.h
        plan/plan_edit.h
        plan/plan_json.h
        plan/plan_lanes.h
        plan/plan_state.h
        plan/section.h
        plan/section_json.h
)
target_link_libraries(ursatz_plan PUBLIC ursatz_model ursatz_theory cjson)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

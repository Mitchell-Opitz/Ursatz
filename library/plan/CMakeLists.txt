# The plan group (R-605): the section description shared by forms, plans and piece requests: its value, vocabulary and rules, and
# its JSON codec over one attribute table. Sources include headers by their path from library/ (plan/<file>.h). No header is
# staged until a file outside the group includes one (the include closure, R-578): P60 B2 stages plan/section.h and
# plan/section_json.h, when the Composer first includes them.
add_library(ursatz_plan
    section.c
)
ursatz_public_headers(ursatz_plan
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
)
target_link_libraries(ursatz_plan PUBLIC ursatz_model ursatz_theory cjson)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

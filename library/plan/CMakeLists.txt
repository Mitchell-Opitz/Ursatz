# The plan group (R-605): the section description shared by forms, plans and piece requests: its value, vocabulary and rules, and
# its JSON codec over one attribute table. Sources include headers by their path from library/ (plan/<file>.h). A header is
# staged when a file outside the group includes it (the include closure, R-578): the Composer includes section.h and section_json.h (forms read their sections through it).
add_library(ursatz_plan
    section.c
    section_json.c
)
ursatz_public_headers(ursatz_plan
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
    HEADERS
        plan/section.h
        plan/section_json.h
)
target_link_libraries(ursatz_plan PUBLIC ursatz_model ursatz_theory cjson)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

# The reading group (spec/target-architecture.md 4.7, R-629): what one reader says about one realization, as claims over it (regions,
# chords, cadences, groups, motive occurrences, meters, lines and voice faults), the builder that makes a reading from a chain run and
# the JSON codec. Sources include headers by their path from library/ (reading/<file>.h). A header is staged when a file outside the
# group includes one (the include closure, R-578): the Analyzer includes them.
add_library(ursatz_reading
    reading.c
    reading_build.c
    reading_json.c
)
ursatz_public_headers(ursatz_reading
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
    HEADERS
        reading/reading.h
        reading/reading_build.h
)
target_link_libraries(ursatz_reading PUBLIC ursatz_realization ursatz_plan ursatz_chain ursatz_analysis ursatz_theory ursatz_model cjson)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

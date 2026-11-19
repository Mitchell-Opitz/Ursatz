# The realization group (spec/target-architecture.md 4.8, 4.11, R-600): lines, performer parts, scoring assignments, movements and
# works. Sources include headers by their path from library/ (realization/<file>.h). No header is staged until a file outside the
# group includes one (the include closure, R-578).
add_library(ursatz_realization
    realization.c
    derivation.c
    line.c
    performer_part.c
    scoring.c
    movement.c
    work.c
    piano_reduction.c
    realization_json.c
)
ursatz_public_headers(ursatz_realization
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
    HEADERS
        realization/derivation.h
        realization/line.h
        realization/movement.h
        realization/performer_part.h
        realization/piano_reduction.h
        realization/realization.h
        realization/scoring.h
        realization/work.h
)
target_link_libraries(ursatz_realization PUBLIC ursatz_model ursatz_theory cjson)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

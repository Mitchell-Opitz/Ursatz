# The workshop (R-641): the process that holds the data folder, its services, the job queue, the history and the events, above the
# two apps and below the GUI. Sources include <ursatz_workshop/...> and the headers beside them.
add_library(ursatz_workshop STATIC
    src/coordinator/workshop.c
)
ursatz_public_headers(ursatz_workshop
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/coordinator
    HEADERS
        ursatz_workshop/workshop_api.h
)
target_link_libraries(ursatz_workshop
    PUBLIC ursatz_common cjson
    PRIVATE ursatz_composer ursatz_analyzer ursatz_theory
)
target_compile_options(ursatz_workshop PRIVATE -Wall -Wextra -Wpedantic)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

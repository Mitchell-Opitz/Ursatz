# The workshop (R-641): the process that holds the data folder, its services, the job queue, the history and the events, above the
# two apps and below the GUI. Sources include <ursatz_workshop/...> and the headers beside them.
add_library(ursatz_workshop STATIC
    src/coordinator/workshop.c
    src/coordinator/settings.c
    src/coordinator/data_lock.c
    src/coordinator/knowledge.c
)
ursatz_public_headers(ursatz_workshop
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/coordinator
    HEADERS
        ursatz_workshop/workshop_api.h
)
# The coordinator's internal header stays visible to the GUI backend until B11 gives it the query and command API.
target_include_directories(ursatz_workshop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/coordinator)
target_link_libraries(ursatz_workshop
    PUBLIC ursatz_common ursatz_composer ursatz_analyzer ursatz_theory cjson
)
target_compile_options(ursatz_workshop PRIVATE -Wall -Wextra -Wpedantic)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

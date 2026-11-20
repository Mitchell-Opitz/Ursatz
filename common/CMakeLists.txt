# The common part (R-638): the helpers every other part shares, on libc and cJSON only. Sources include <common/...>.
add_library(ursatz_common STATIC
    status.c
    files.c
    strings.c
    json.c
    hash.c
    events.c
)
ursatz_public_headers(ursatz_common
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/..
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/..
    HEADERS
        common/status.h
        common/files.h
        common/strings.h
        common/json.h
        common/hash.h
        common/job.h
        common/events.h
)
# floor in the JSON number readers
target_link_libraries(ursatz_common PUBLIC cjson m)
find_package(Threads REQUIRED)
target_link_libraries(ursatz_common PUBLIC Threads::Threads)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

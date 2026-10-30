add_library(western_periods
    framework.c
    periods.c
)
# No app, chain or framework source includes a header of western-periods yet, so nothing is public (R-223).
ursatz_public_headers(western_periods
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX western-periods
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(western_periods PUBLIC ursatz)

ursatz_add_tests()

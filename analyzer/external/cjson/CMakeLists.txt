# cJSON v1.7.18 (MIT, https://github.com/DaveGamble/cJSON), vendored unmodified.
add_library(cjson STATIC cJSON.c)
target_include_directories(cjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

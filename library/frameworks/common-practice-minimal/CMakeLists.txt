add_library(common_practice_minimal
    framework.c
    chord_vocabulary.c
    key_vocabulary.c
    triad_identification.c
    key_identification.c
)
target_include_directories(common_practice_minimal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(common_practice_minimal PUBLIC ursatz)

enable_testing()
add_subdirectory(tests)
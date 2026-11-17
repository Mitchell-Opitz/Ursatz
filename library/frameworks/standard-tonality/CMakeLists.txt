add_library(standard_tonality
    analysis/chord_input.c
    analysis/roman_numeral_adapter.c
    analysis/applied_chords.c
)
# Public headers (R-223), staged under standard-tonality/ in the build tree; the source directories are private.
ursatz_public_headers(standard_tonality
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX standard-tonality
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    HEADERS
        analysis/applied_chords.h
        analysis/chord_input.h
        analysis/roman_numeral_adapter.h
)
target_link_libraries(standard_tonality PUBLIC ursatz)
# An analysis file includes its former framework's theory headers by their path from library/.
target_include_directories(standard_tonality PRIVATE ${PROJECT_SOURCE_DIR})

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()

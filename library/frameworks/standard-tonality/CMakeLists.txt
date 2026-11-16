add_library(standard_tonality
    framework.c
    tonal_key.c
    triad_rules.c
    triad_in_key_rules.c
    seventh_rules.c
    seventh_in_key_rules.c
    roman_numeral_rules.c
    roman_numeral_grammar.c
    roman_numeral_analysis_rules.c
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
        roman_numeral_analysis_rules.h
        roman_numeral_grammar.h
        roman_numeral_rules.h
        seventh_rules.h
        tonal_key.h
        triad_rules.h
)
target_link_libraries(standard_tonality PUBLIC ursatz)

ursatz_add_tests()

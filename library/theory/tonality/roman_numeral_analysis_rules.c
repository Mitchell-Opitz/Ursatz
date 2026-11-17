#include "theory/tonality/roman_numeral_analysis_rules.h"

#include <stddef.h>
#include <string.h>

#include "theory/tonality/seventh_in_key_rules.h"
#include "theory/tonality/triad_in_key_rules.h"
#include "model/pitch/pitch_arithmetic.h"

static bool valid_mode(StandardTonalityRomanKeyMode mode) {
    return mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR || mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
}

typedef struct {
    PitchLetter letter;
    int accidental;
} NoteName;

/* QUALITY: the triad quality the key gives the degree. In minor a root on
   a natural degree takes the natural quality or, below the seventh, the
   one the raised leading tone gives (the harmonic minor's); a raised
   leading tone root takes only the harmonic minor's seventh-degree
   quality. */
static bool triad_in_key(StandardTonalityRomanKeyMode mode, int degree, bool raised,
                         StandardTonalityTriadQuality quality) {
    StandardTonalityTriadQuality table;
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR) {
        return standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MAJOR, degree, &table) &&
               table == quality;
    }
    if (raised) {
        return standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, degree, &table) &&
               table == quality;
    }
    if (standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL, degree, &table) &&
        table == quality) {
        return true;
    }
    return degree < 7 &&
           standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, degree, &table) &&
           table == quality;
}

static bool seventh_in_key(StandardTonalityRomanKeyMode mode, int degree, bool raised,
                           StandardTonalitySeventhQuality quality) {
    StandardTonalitySeventhQuality table;
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR) {
        return standard_tonality_seventh_quality_in_key(STANDARD_TONALITY_SEVENTH_KEY_MAJOR, degree, &table) &&
               table == quality;
    }
    if (raised) {
        return standard_tonality_seventh_quality_in_key(STANDARD_TONALITY_SEVENTH_KEY_MINOR_RAISED_LEADING_TONE, degree,
                                                        &table) &&
               table == quality;
    }
    if (standard_tonality_seventh_quality_in_key(STANDARD_TONALITY_SEVENTH_KEY_MINOR_NATURAL, degree, &table) &&
        table == quality) {
        return true;
    }
    return degree < 7 &&
           standard_tonality_seventh_quality_in_key(STANDARD_TONALITY_SEVENTH_KEY_MINOR_RAISED_LEADING_TONE, degree,
                                                    &table) &&
           table == quality;
}

bool standard_tonality_roman_numeral_fits_key(StandardTonalityRomanKeyMode mode, int degree, bool raised_leading_tone,
                                              const StandardTonalityRomanNumeral *numeral) {
    if (numeral == NULL || !valid_mode(mode) || degree < 1 || degree > 7 || numeral->degree != degree) {
        return false;
    }
    return numeral->seventh ? seventh_in_key(mode, degree, raised_leading_tone, numeral->seventh_quality)
                            : triad_in_key(mode, degree, raised_leading_tone, numeral->triad);
}


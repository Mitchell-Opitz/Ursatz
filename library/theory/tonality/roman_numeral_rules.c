#include "theory/tonality/roman_numeral_rules.h"

#include <stdio.h>
#include <string.h>

#include "theory/tonality/roman_numeral_grammar.h"

static const char *const UPPERCASE[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
static const char *const LOWERCASE[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};

const char *standard_tonality_roman_numeral_degree(int degree, bool uppercase) {
    if (degree < 1 || degree > 7) {
        return NULL;
    }
    return uppercase ? UPPERCASE[degree - 1] : LOWERCASE[degree - 1];
}

int standard_tonality_roman_numeral_degree_of(const char *letters, size_t length) {
    char lower[8];
    if (letters == NULL || length == 0 || length >= sizeof(lower)) {
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        char c = letters[i];
        if (c != 'i' && c != 'I' && c != 'v' && c != 'V') {
            return 0;
        }
        lower[i] = (char)(c | 0x20);
    }
    lower[length] = '\0';
    for (int degree = 0; degree < 7; degree++) {
        if (strcmp(lower, LOWERCASE[degree]) == 0) {
            return degree + 1;
        }
    }
    return 0;
}

const char *standard_tonality_roman_numeral_triad_symbol(StandardTonalityTriadQuality quality) {
    switch (quality) {
        case STANDARD_TONALITY_TRIAD_MAJOR:
        case STANDARD_TONALITY_TRIAD_MINOR:
            return "";
        case STANDARD_TONALITY_TRIAD_DIMINISHED:
            return "\xC2\xB0";
        case STANDARD_TONALITY_TRIAD_AUGMENTED:
            return "+";
        default:
            return NULL;
    }
}

const char *standard_tonality_roman_numeral_seventh_symbol(StandardTonalitySeventhQuality quality) {
    switch (quality) {
        case STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR:
        case STANDARD_TONALITY_SEVENTH_MAJOR_MINOR:
        case STANDARD_TONALITY_SEVENTH_MINOR_MINOR:
            return "7";
        case STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED:
            return "\xE2\x88\x85" "7";
        case STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED:
            return "\xC2\xB0" "7";
        default:
            return NULL;
    }
}

bool standard_tonality_roman_numeral_inferred_seventh(int degree, StandardTonalitySeventhQuality *quality) {
    if (degree < 1 || degree > 7 || quality == NULL) {
        return false;
    }
    *quality = (degree == 5 || degree == 7) ? STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
                                            : STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR;
    return true;
}

bool standard_tonality_roman_numeral_triad(int degree, StandardTonalityTriadQuality quality,
                                           StandardTonalityRomanNumeral *out) {
    if (out == NULL || degree < 1 || degree > 7 || standard_tonality_roman_numeral_triad_symbol(quality) == NULL) {
        return false;
    }
    out->degree = degree;
    out->seventh = false;
    out->triad = quality;
    out->seventh_quality = STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR;
    return true;
}

bool standard_tonality_roman_numeral_seventh_chord(int degree, StandardTonalitySeventhQuality quality,
                                                   StandardTonalityRomanNumeral *out) {
    if (out == NULL || degree < 1 || degree > 7 || standard_tonality_roman_numeral_seventh_symbol(quality) == NULL) {
        return false;
    }
    out->degree = degree;
    out->seventh = true;
    out->triad = STANDARD_TONALITY_TRIAD_MAJOR;
    out->seventh_quality = quality;
    return true;
}

static bool valid_numeral(const StandardTonalityRomanNumeral *numeral) {
    if (numeral == NULL || numeral->degree < 1 || numeral->degree > 7) {
        return false;
    }
    return numeral->seventh ? standard_tonality_roman_numeral_seventh_symbol(numeral->seventh_quality) != NULL
                            : standard_tonality_roman_numeral_triad_symbol(numeral->triad) != NULL;
}

bool standard_tonality_roman_numeral_is_unambiguous(const StandardTonalityRomanNumeral *numeral) {
    if (!valid_numeral(numeral)) {
        return false;
    }
    if (!numeral->seventh) {
        return true;
    }
    if (numeral->seventh_quality != STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR &&
        numeral->seventh_quality != STANDARD_TONALITY_SEVENTH_MAJOR_MINOR) {
        return true;
    }
    StandardTonalitySeventhQuality inferred;
    return standard_tonality_roman_numeral_inferred_seventh(numeral->degree, &inferred) &&
           inferred == numeral->seventh_quality;
}

bool standard_tonality_roman_numeral_format(const StandardTonalityRomanNumeral *numeral, char *buffer, size_t size) {
    StandardTonalityNumeralText text;
    return standard_tonality_numeral_from_base(numeral, &text) &&
           standard_tonality_numeral_format_styled(&text, STANDARD_TONALITY_NUMERAL_STYLE_DISPLAY, buffer, size);
}


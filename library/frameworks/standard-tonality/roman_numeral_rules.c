#include "roman_numeral_rules.h"

#include <stdio.h>
#include <string.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT] = {
    "roman-numeral-degrees", "roman-numeral-case", "roman-numeral-triad-symbols", "roman-numeral-seventh-symbols",
    "roman-numeral-seventh-inference"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT];

static bool valid_rule(StandardTonalityRomanNumeralRule which) {
    return which >= 0 && which < STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT;
}

static void ensure_built(StandardTonalityRomanNumeralRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_roman_numeral_rule(StandardTonalityRomanNumeralRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_roman_numeral_constraint(StandardTonalityRomanNumeralRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static const char *const UPPERCASE[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
static const char *const LOWERCASE[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};

const char *standard_tonality_roman_numeral_degree(int degree, bool uppercase) {
    if (degree < 1 || degree > 7) {
        return NULL;
    }
    return uppercase ? UPPERCASE[degree - 1] : LOWERCASE[degree - 1];
}

bool standard_tonality_roman_numeral_triad_uppercase(StandardTonalityTriadQuality quality) {
    return quality == STANDARD_TONALITY_TRIAD_MAJOR || quality == STANDARD_TONALITY_TRIAD_AUGMENTED;
}

bool standard_tonality_roman_numeral_seventh_uppercase(StandardTonalitySeventhQuality quality) {
    return quality == STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR || quality == STANDARD_TONALITY_SEVENTH_MAJOR_MINOR;
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
    if (buffer == NULL || size == 0 || !valid_numeral(numeral)) {
        return false;
    }
    bool uppercase = numeral->seventh ? standard_tonality_roman_numeral_seventh_uppercase(numeral->seventh_quality)
                                      : standard_tonality_roman_numeral_triad_uppercase(numeral->triad);
    const char *symbol = numeral->seventh ? standard_tonality_roman_numeral_seventh_symbol(numeral->seventh_quality)
                                          : standard_tonality_roman_numeral_triad_symbol(numeral->triad);

    char text[32];
    int length = snprintf(text, sizeof(text), "%s%s", standard_tonality_roman_numeral_degree(numeral->degree, uppercase),
                          symbol);
    if (length < 0 || (size_t)length >= sizeof(text) || (size_t)length >= size) {
        return false;
    }
    memcpy(buffer, text, (size_t)length + 1);
    return true;
}

/* Reads the numeral letters at the start of the text: the degree and
   whether they were uppercase. Longest first, so VII is not VI then I.
   Returns the bytes taken, or 0. */
static size_t parse_degree(const char *text, int *degree, bool *uppercase) {
    for (int d = 7; d >= 1; d--) {
        size_t upper_length = strlen(UPPERCASE[d - 1]);
        if (strncmp(text, UPPERCASE[d - 1], upper_length) == 0) {
            *degree = d;
            *uppercase = true;
            return upper_length;
        }
        size_t lower_length = strlen(LOWERCASE[d - 1]);
        if (strncmp(text, LOWERCASE[d - 1], lower_length) == 0) {
            *degree = d;
            *uppercase = false;
            return lower_length;
        }
    }
    return 0;
}

bool standard_tonality_roman_numeral_parse(const char *text, StandardTonalityRomanNumeral *out) {
    if (text == NULL || out == NULL) {
        return false;
    }
    int degree;
    bool uppercase;
    size_t taken = parse_degree(text, &degree, &uppercase);
    if (taken == 0) {
        return false;
    }
    const char *rest = text + taken;

    if (uppercase) {
        if (rest[0] == '\0') {
            return standard_tonality_roman_numeral_triad(degree, STANDARD_TONALITY_TRIAD_MAJOR, out);
        }
        if (strcmp(rest, "+") == 0) {
            return standard_tonality_roman_numeral_triad(degree, STANDARD_TONALITY_TRIAD_AUGMENTED, out);
        }
        if (strcmp(rest, "7") == 0) {
            StandardTonalitySeventhQuality inferred;
            return standard_tonality_roman_numeral_inferred_seventh(degree, &inferred) &&
                   standard_tonality_roman_numeral_seventh_chord(degree, inferred, out);
        }
        return false;
    }

    if (rest[0] == '\0') {
        return standard_tonality_roman_numeral_triad(degree, STANDARD_TONALITY_TRIAD_MINOR, out);
    }
    if (strcmp(rest, "\xC2\xB0") == 0 || strcmp(rest, "o") == 0) {
        return standard_tonality_roman_numeral_triad(degree, STANDARD_TONALITY_TRIAD_DIMINISHED, out);
    }
    if (strcmp(rest, "7") == 0) {
        return standard_tonality_roman_numeral_seventh_chord(degree, STANDARD_TONALITY_SEVENTH_MINOR_MINOR, out);
    }
    if (strcmp(rest, "\xE2\x88\x85" "7") == 0 || strcmp(rest, "\xC3\xB8" "7") == 0) {
        return standard_tonality_roman_numeral_seventh_chord(degree, STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED, out);
    }
    if (strcmp(rest, "\xC2\xB0" "7") == 0 || strcmp(rest, "o7") == 0) {
        return standard_tonality_roman_numeral_seventh_chord(degree, STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED, out);
    }
    return false;
}

ChordResult standard_tonality_build_roman_numeral_chord(const StandardTonalityRomanNumeral *numeral,
                                                        EntityID *target_id, Chord **out) {
    char label[32];
    if (!standard_tonality_roman_numeral_format(numeral, label, sizeof(label))) {
        return CHORD_ERR_NULL_LABEL;
    }
    return standard_tonality_build_chord_from_label(label, "roman-numeral-vocabulary", target_id, out);
}

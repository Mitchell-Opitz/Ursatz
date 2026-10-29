#include "roman_numeral_rules.h"

#include <stdio.h>
#include <string.h>

#include "roman_numeral_grammar.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT] = {
    "roman-numeral-degrees", "roman-numeral-case", "roman-numeral-triad-symbols", "roman-numeral-seventh-symbols",
    "roman-numeral-seventh-inference"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT);

static bool valid_rule(StandardTonalityRomanNumeralRule which) {
    return which >= 0 && which < STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT;
}

const Rule *standard_tonality_roman_numeral_rule(StandardTonalityRomanNumeralRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_roman_numeral_constraint(StandardTonalityRomanNumeralRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
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
    StandardTonalityNumeralText text;
    return standard_tonality_numeral_from_base(numeral, &text) &&
           standard_tonality_numeral_format_styled(&text, STANDARD_TONALITY_NUMERAL_STYLE_DISPLAY, buffer, size);
}

bool standard_tonality_roman_numeral_parse(const char *text, StandardTonalityRomanNumeral *out) {
    StandardTonalityNumeralText parsed;
    /* The plain numeral has no place for an alteration, an inversion, or an applied target. */
    return standard_tonality_numeral_parse(text, &parsed) && parsed.alteration == 0 && parsed.inversion == 0 &&
           !parsed.has_target && standard_tonality_numeral_base(&parsed, out);
}

ChordResult standard_tonality_build_roman_numeral_chord(const StandardTonalityRomanNumeral *numeral,
                                                        EntityID *target_id, Chord **out) {
    char label[32];
    if (!standard_tonality_roman_numeral_format(numeral, label, sizeof(label))) {
        return CHORD_ERR_NULL_LABEL;
    }
    return standard_tonality_build_chord_from_label(label, "roman-numeral-vocabulary", target_id, out);
}

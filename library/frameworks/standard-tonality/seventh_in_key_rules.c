#include "seventh_in_key_rules.h"

#include <stddef.h>

#include "major_scale_rules.h"
#include "minor_scale_rules.h"
#include "rule_pair.h"
#include "seventh_spelling_rules.h"

static const char *const NAMES[STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_COUNT] = {
    "seventh-in-key-major-key", "seventh-in-key-minor-key", "seventh-in-key-raised-leading-tone",
    "seventh-in-key-key-independent"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_seventh_in_key, StandardTonalitySeventhInKeyRule, STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_COUNT, g_table)

#define MAMA STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR
#define MAMI STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
#define MIMI STANDARD_TONALITY_SEVENTH_MINOR_MINOR
#define HDIM STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED
#define FDIM STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED

static const StandardTonalitySeventhQuality QUALITIES[STANDARD_TONALITY_SEVENTH_KEY_COUNT][7] = {
    {MAMA, MIMI, MIMI, MAMA, MAMI, MIMI, HDIM}, /* major */
    {MIMI, HDIM, MAMA, MIMI, MIMI, MAMA, MAMI}, /* natural minor */
    {MIMI, HDIM, MAMA, MIMI, MAMI, MAMA, FDIM}, /* minor, raised leading tone */
};

static bool valid_key(StandardTonalitySeventhKey key) {
    return key >= 0 && key < STANDARD_TONALITY_SEVENTH_KEY_COUNT;
}

bool standard_tonality_seventh_quality_in_key(StandardTonalitySeventhKey key, int degree,
                                              StandardTonalitySeventhQuality *quality) {
    if (!valid_key(key) || degree < 1 || degree > 7 || quality == NULL) {
        return false;
    }
    *quality = QUALITIES[key][degree - 1];
    return true;
}

bool standard_tonality_spell_diatonic_seventh(StandardTonalitySeventhKey key, const PitchSpelling *tonic, int degree,
                                              PitchSpelling *out[4]) {
    if (!valid_key(key) || tonic == NULL || degree < 1 || degree > 7 || out == NULL) {
        return false;
    }

    PitchSpelling *scale[8];
    bool built;
    switch (key) {
        case STANDARD_TONALITY_SEVENTH_KEY_MAJOR:
            built = standard_tonality_build_major_scale(tonic, scale);
            break;
        case STANDARD_TONALITY_SEVENTH_KEY_MINOR_NATURAL:
            built = standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, tonic, scale);
            break;
        default:
            built = standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_HARMONIC, tonic, scale);
            break;
    }
    if (!built) {
        return false;
    }

    bool ok = standard_tonality_spell_seventh(scale[degree - 1], QUALITIES[key][degree - 1], out);

    for (size_t i = 0; i < 8; i++) {
        pitch_spelling_destroy(scale[i]);
    }
    return ok;
}

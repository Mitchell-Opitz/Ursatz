#include "interval_white_key_rules.h"

#include <stddef.h>

#include "interval_inversion_rules.h"
#include "interval_size_rules.h"
#include "rule_pair.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_WHITE_KEY_RULE_COUNT] = {
    "white-key-seconds", "white-key-thirds", "white-key-fourths", "white-key-by-inversion", "white-key-alteration"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_WHITE_KEY_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_white_key, StandardTonalityWhiteKeyRule, STANDARD_TONALITY_WHITE_KEY_RULE_COUNT, g_table)

/* The letter `size - 1` letters above `letter`. */
static PitchLetter letter_above(PitchLetter letter, int size) {
    for (int i = 1; i < size; i++) {
        letter = standard_tonality_next_letter(letter);
    }
    return letter;
}

/* Seconds, thirds, fourths: the memorized rules. */
static StandardTonalityIntervalQuality memorized_quality(PitchLetter lower, int size) {
    switch (size) {
        case 2:
            /* Major, except E-F and B-C. */
            return (lower == PITCH_LETTER_E || lower == PITCH_LETTER_B) ? STANDARD_TONALITY_QUALITY_MINOR
                                                                         : STANDARD_TONALITY_QUALITY_MAJOR;
        case 3:
            /* Minor, except C-E, F-A, G-B. */
            return (lower == PITCH_LETTER_C || lower == PITCH_LETTER_F || lower == PITCH_LETTER_G)
                       ? STANDARD_TONALITY_QUALITY_MAJOR
                       : STANDARD_TONALITY_QUALITY_MINOR;
        case 4:
            /* Perfect, except F-B. */
            return lower == PITCH_LETTER_F ? STANDARD_TONALITY_QUALITY_AUGMENTED : STANDARD_TONALITY_QUALITY_PERFECT;
        default:
            return STANDARD_TONALITY_QUALITY_PERFECT;
    }
}

bool standard_tonality_white_key_quality(PitchLetter lower, int size, StandardTonalityIntervalQuality *quality) {
    if (quality == NULL || size < 1 || size > 8) {
        return false;
    }

    if (size == 1 || size == 8) {
        *quality = STANDARD_TONALITY_QUALITY_PERFECT;
    } else if (size <= 4) {
        *quality = memorized_quality(lower, size);
    } else {
        /* Fifths, sixths, sevenths: invert the interval from the upper
           note down to the next lower note's octave -- from the upper
           letter, a size of 9 - size. */
        PitchLetter upper = letter_above(lower, size);
        StandardTonalityIntervalQuality inverse = memorized_quality(upper, 9 - size);
        *quality = standard_tonality_interval_inverted_quality(inverse);
    }
    return true;
}

bool standard_tonality_white_key_default_quality(int size, StandardTonalityIntervalQuality *quality) {
    if (quality == NULL || size < 1 || size > 8) {
        return false;
    }
    switch (size) {
        case 2:
        case 6:
            *quality = STANDARD_TONALITY_QUALITY_MAJOR;
            break;
        case 3:
        case 7:
            *quality = STANDARD_TONALITY_QUALITY_MINOR;
            break;
        default:
            *quality = STANDARD_TONALITY_QUALITY_PERFECT;
            break;
    }
    return true;
}

bool standard_tonality_white_key_is_exception(PitchLetter lower, int size) {
    StandardTonalityIntervalQuality actual;
    StandardTonalityIntervalQuality expected;
    if (!standard_tonality_white_key_quality(lower, size, &actual) ||
        !standard_tonality_white_key_default_quality(size, &expected)) {
        return false;
    }
    return actual != expected;
}

bool standard_tonality_white_key_altered_quality(PitchLetter lower, int lower_accidental, int size,
                                                 int upper_accidental, StandardTonalityIntervalQuality *quality) {
    StandardTonalityIntervalQuality base;
    if (quality == NULL || !standard_tonality_white_key_quality(lower, size, &base)) {
        return false;
    }

    StandardTonalityIntervalQuality shifted;
    if (!standard_tonality_interval_quality_shift(size, base, upper_accidental - lower_accidental, &shifted)) {
        return false;
    }

    int semitones;
    if (!standard_tonality_interval_semitones(size, shifted, &semitones) || semitones < 0) {
        return false;
    }
    *quality = shifted;
    return true;
}

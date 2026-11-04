#include "mode_rules.h"

#include <stddef.h>

#include "major_scale_rules.h"
#include "rule_pair.h"
#include "scale_pattern.h"

static const char *const NAMES[STANDARD_TONALITY_MODE_RULE_COUNT] = {"mode-patterns", "mode-brightness", "mode-third",
                                                                      "mode-alterations", "mode-parallel"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_MODE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_mode, StandardTonalityModeRule, STANDARD_TONALITY_MODE_RULE_COUNT, g_table)

static bool valid_mode(StandardTonalityMode mode) {
    return mode >= 0 && mode < STANDARD_TONALITY_MODE_COUNT;
}

static const char *const MODE_NAMES[STANDARD_TONALITY_MODE_COUNT] = {"lydian",  "ionian",   "mixolydian", "dorian",
                                                                      "aeolian", "phrygian", "locrian"};

const char *standard_tonality_mode_name(StandardTonalityMode mode) {
    return valid_mode(mode) ? MODE_NAMES[mode] : NULL;
}

static const int STEPS[STANDARD_TONALITY_MODE_COUNT][STANDARD_TONALITY_MODE_STEP_COUNT] = {
    {2, 2, 2, 1, 2, 2, 1}, /* lydian */
    {2, 2, 1, 2, 2, 2, 1}, /* ionian */
    {2, 2, 1, 2, 2, 1, 2}, /* mixolydian */
    {2, 1, 2, 2, 2, 1, 2}, /* dorian */
    {2, 1, 2, 2, 1, 2, 2}, /* aeolian */
    {1, 2, 2, 2, 1, 2, 2}, /* phrygian */
    {1, 2, 2, 1, 2, 2, 2}, /* locrian */
};

int standard_tonality_mode_step(StandardTonalityMode mode, size_t index) {
    return STEPS[mode][index];
}

bool standard_tonality_is_mode_scale(StandardTonalityMode mode, const PitchSpelling *const *notes, size_t count) {
    if (!valid_mode(mode)) {
        return false;
    }
    return standard_tonality_is_scale_pattern(STEPS[mode], STANDARD_TONALITY_MODE_STEP_COUNT, notes, count);
}

bool standard_tonality_build_mode_scale(StandardTonalityMode mode, const PitchSpelling *tonic,
                                        PitchSpelling *out[STANDARD_TONALITY_MODE_NOTE_COUNT]) {
    if (!valid_mode(mode)) {
        return false;
    }
    return standard_tonality_build_scale_pattern(STEPS[mode], STANDARD_TONALITY_MODE_STEP_COUNT, tonic, out);
}

int standard_tonality_mode_brightness_rank(StandardTonalityMode mode) {
    return valid_mode(mode) ? (int)mode + 1 : 0;
}

bool standard_tonality_mode_is_brighter(StandardTonalityMode a, StandardTonalityMode b) {
    return valid_mode(a) && valid_mode(b) && a < b;
}

int standard_tonality_mode_third_semitones(StandardTonalityMode mode) {
    if (!valid_mode(mode)) {
        return 0;
    }
    return STEPS[mode][0] + STEPS[mode][1];
}

bool standard_tonality_mode_is_bright(StandardTonalityMode mode) {
    return standard_tonality_mode_third_semitones(mode) == 4;
}

bool standard_tonality_mode_is_dark(StandardTonalityMode mode) {
    return standard_tonality_mode_third_semitones(mode) == 3;
}

size_t standard_tonality_mode_alterations(StandardTonalityMode mode, StandardTonalityModeReference reference,
                                          int degrees[6], int offsets[6]) {
    if (!valid_mode(mode) || degrees == NULL || offsets == NULL) {
        return 0;
    }
    StandardTonalityMode base;
    switch (reference) {
        case STANDARD_TONALITY_MODE_REFERENCE_IONIAN:
            base = STANDARD_TONALITY_MODE_IONIAN;
            break;
        case STANDARD_TONALITY_MODE_REFERENCE_AEOLIAN:
            base = STANDARD_TONALITY_MODE_AEOLIAN;
            break;
        default:
            return 0;
    }

    /* Degree n sits at the sum of the first n - 1 steps above the tonic. */
    size_t count = 0;
    int position = 0;
    int base_position = 0;
    for (size_t i = 0; i < STANDARD_TONALITY_MODE_STEP_COUNT - 1; i++) {
        position += STEPS[mode][i];
        base_position += STEPS[base][i];
        if (position != base_position) {
            degrees[count] = (int)i + 2;
            offsets[count] = position - base_position;
            count++;
        }
    }
    return count;
}

bool standard_tonality_modes_parallel(const PitchSpelling *tonic_a, const PitchSpelling *tonic_b) {
    return standard_tonality_same_scale_name(tonic_a, tonic_b);
}

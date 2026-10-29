#include "seventh_spelling_rules.h"

#include <stddef.h>

#include "interval_spelling_rules.h"
#include "major_scale_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_SEVENTH_SPELLING_RULE_COUNT] = {"seventh-spelling-key-signature",
                                                                                  "seventh-spelling-alteration"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_SEVENTH_SPELLING_RULE_COUNT);

static bool valid_rule(StandardTonalitySeventhSpellingRule which) {
    return which >= 0 && which < STANDARD_TONALITY_SEVENTH_SPELLING_RULE_COUNT;
}

const Rule *standard_tonality_seventh_spelling_rule(StandardTonalitySeventhSpellingRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_seventh_spelling_constraint(StandardTonalitySeventhSpellingRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_tonality_seventh_alteration(StandardTonalitySeventhQuality quality, int *third_shift, int *fifth_shift,
                                          int *seventh_shift) {
    if (third_shift == NULL || fifth_shift == NULL || seventh_shift == NULL) {
        return false;
    }
    switch (quality) {
        case STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR:
            *third_shift = 0;
            *fifth_shift = 0;
            *seventh_shift = 0;
            return true;
        case STANDARD_TONALITY_SEVENTH_MAJOR_MINOR:
            *third_shift = 0;
            *fifth_shift = 0;
            *seventh_shift = -1;
            return true;
        case STANDARD_TONALITY_SEVENTH_MINOR_MINOR:
            *third_shift = -1;
            *fifth_shift = 0;
            *seventh_shift = -1;
            return true;
        case STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED:
            *third_shift = -1;
            *fifth_shift = -1;
            *seventh_shift = -1;
            return true;
        case STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED:
            *third_shift = -1;
            *fifth_shift = -1;
            *seventh_shift = -2;
            return true;
        default:
            return false;
    }
}

static bool copy_spelling(const PitchSpelling *source, PitchSpelling **out) {
    return pitch_spelling_create(pitch_spelling_letter(source), pitch_spelling_accidental(source),
                                 pitch_spelling_octave(source), out) == PITCH_SPELLING_OK;
}

static void destroy_all(PitchSpelling *notes[4]) {
    for (size_t i = 0; i < 4; i++) {
        if (notes[i] != NULL) {
            pitch_spelling_destroy(notes[i]);
        }
    }
}

bool standard_tonality_spell_seventh(const PitchSpelling *root, StandardTonalitySeventhQuality quality,
                                     PitchSpelling *out[4]) {
    StandardTonalityTriadQuality triad;
    StandardTonalityIntervalQuality seventh_quality;
    StandardTonalityIntervalQuality third_quality;
    StandardTonalityIntervalQuality fifth_quality;
    if (root == NULL || out == NULL || !standard_tonality_seventh_parts(quality, &triad, &seventh_quality) ||
        !standard_tonality_triad_intervals(triad, &third_quality, &fifth_quality)) {
        return false;
    }

    PitchSpelling *built[4] = {NULL, NULL, NULL, NULL};
    if (!copy_spelling(root, &built[0]) || !standard_tonality_spell_above(root, 3, third_quality, &built[1]) ||
        !standard_tonality_spell_above(root, 5, fifth_quality, &built[2]) ||
        !standard_tonality_spell_above(root, 7, seventh_quality, &built[3])) {
        destroy_all(built);
        return false;
    }

    for (size_t i = 0; i < 4; i++) {
        out[i] = built[i];
    }
    return true;
}

bool standard_tonality_spell_seventh_by_key_signature(const PitchSpelling *root,
                                                      StandardTonalitySeventhQuality quality, PitchSpelling *out[4]) {
    int third_shift;
    int fifth_shift;
    int seventh_shift;
    if (root == NULL || out == NULL ||
        !standard_tonality_seventh_alteration(quality, &third_shift, &fifth_shift, &seventh_shift)) {
        return false;
    }

    /* The root's key: its major scale supplies the third, fifth, and seventh. */
    PitchSpelling *scale[STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT];
    if (!standard_tonality_build_major_scale(root, scale)) {
        return false;
    }

    int accidentals[4] = {pitch_spelling_accidental(scale[0]), pitch_spelling_accidental(scale[2]) + third_shift,
                          pitch_spelling_accidental(scale[4]) + fifth_shift,
                          pitch_spelling_accidental(scale[6]) + seventh_shift};
    const PitchSpelling *sources[4] = {scale[0], scale[2], scale[4], scale[6]};

    PitchSpelling *built[4] = {NULL, NULL, NULL, NULL};
    bool ok = true;
    for (size_t i = 0; i < 4 && ok; i++) {
        ok = accidentals[i] >= -2 && accidentals[i] <= 2 &&
             pitch_spelling_create(pitch_spelling_letter(sources[i]), accidentals[i], pitch_spelling_octave(sources[i]),
                                   &built[i]) == PITCH_SPELLING_OK;
    }

    for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT; i++) {
        pitch_spelling_destroy(scale[i]);
    }

    if (!ok) {
        destroy_all(built);
        return false;
    }
    for (size_t i = 0; i < 4; i++) {
        out[i] = built[i];
    }
    return true;
}

#include "triad_spelling_rules.h"

#include <stddef.h>

#include "interval_spelling_rules.h"
#include "major_scale_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_TRIAD_SPELLING_RULE_COUNT] = {"triad-spelling-key-signature",
                                                                                "triad-spelling-alteration"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_TRIAD_SPELLING_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_TRIAD_SPELLING_RULE_COUNT];

static bool valid_rule(StandardTonalityTriadSpellingRule which) {
    return which >= 0 && which < STANDARD_TONALITY_TRIAD_SPELLING_RULE_COUNT;
}

static void ensure_built(StandardTonalityTriadSpellingRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_triad_spelling_rule(StandardTonalityTriadSpellingRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_triad_spelling_constraint(StandardTonalityTriadSpellingRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_tonality_triad_alteration(StandardTonalityTriadQuality quality, int *third_shift, int *fifth_shift) {
    if (third_shift == NULL || fifth_shift == NULL) {
        return false;
    }
    switch (quality) {
        case STANDARD_TONALITY_TRIAD_MAJOR:
            *third_shift = 0;
            *fifth_shift = 0;
            return true;
        case STANDARD_TONALITY_TRIAD_MINOR:
            *third_shift = -1;
            *fifth_shift = 0;
            return true;
        case STANDARD_TONALITY_TRIAD_DIMINISHED:
            *third_shift = -1;
            *fifth_shift = -1;
            return true;
        case STANDARD_TONALITY_TRIAD_AUGMENTED:
            *third_shift = 0;
            *fifth_shift = 1;
            return true;
        default:
            return false;
    }
}

static bool copy_spelling(const PitchSpelling *source, PitchSpelling **out) {
    return pitch_spelling_create(pitch_spelling_letter(source), pitch_spelling_accidental(source),
                                 pitch_spelling_octave(source), out) == PITCH_SPELLING_OK;
}

bool standard_tonality_spell_triad(const PitchSpelling *root, StandardTonalityTriadQuality quality,
                                   PitchSpelling *out[3]) {
    StandardTonalityIntervalQuality third_quality;
    StandardTonalityIntervalQuality fifth_quality;
    if (root == NULL || out == NULL || !standard_tonality_triad_intervals(quality, &third_quality, &fifth_quality)) {
        return false;
    }

    PitchSpelling *built[3] = {NULL, NULL, NULL};
    if (!copy_spelling(root, &built[0]) || !standard_tonality_spell_above(root, 3, third_quality, &built[1]) ||
        !standard_tonality_spell_above(root, 5, fifth_quality, &built[2])) {
        for (size_t i = 0; i < 3; i++) {
            if (built[i] != NULL) {
                pitch_spelling_destroy(built[i]);
            }
        }
        return false;
    }

    for (size_t i = 0; i < 3; i++) {
        out[i] = built[i];
    }
    return true;
}

bool standard_tonality_spell_triad_by_key_signature(const PitchSpelling *root, StandardTonalityTriadQuality quality,
                                                    PitchSpelling *out[3]) {
    int third_shift;
    int fifth_shift;
    if (root == NULL || out == NULL || !standard_tonality_triad_alteration(quality, &third_shift, &fifth_shift)) {
        return false;
    }

    /* The root's key: its major scale supplies the third and fifth. */
    PitchSpelling *scale[STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT];
    if (!standard_tonality_build_major_scale(root, scale)) {
        return false;
    }

    int accidentals[3] = {pitch_spelling_accidental(scale[0]), pitch_spelling_accidental(scale[2]) + third_shift,
                          pitch_spelling_accidental(scale[4]) + fifth_shift};
    const PitchSpelling *sources[3] = {scale[0], scale[2], scale[4]};

    PitchSpelling *built[3] = {NULL, NULL, NULL};
    bool ok = true;
    for (size_t i = 0; i < 3 && ok; i++) {
        ok = accidentals[i] >= -2 && accidentals[i] <= 2 &&
             pitch_spelling_create(pitch_spelling_letter(sources[i]), accidentals[i], pitch_spelling_octave(sources[i]),
                                   &built[i]) == PITCH_SPELLING_OK;
    }

    for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT; i++) {
        pitch_spelling_destroy(scale[i]);
    }

    if (!ok) {
        for (size_t i = 0; i < 3; i++) {
            if (built[i] != NULL) {
                pitch_spelling_destroy(built[i]);
            }
        }
        return false;
    }
    for (size_t i = 0; i < 3; i++) {
        out[i] = built[i];
    }
    return true;
}

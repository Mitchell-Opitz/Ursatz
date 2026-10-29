#include "interval_spelling_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "tonal_pitch.h"
#include "pitch/pitch_arithmetic.h"

static const char *const NAMES[STANDARD_TONALITY_INTERVAL_SPELLING_RULE_COUNT] = {"interval-spelling-above"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_INTERVAL_SPELLING_RULE_COUNT);

static bool valid_rule(StandardTonalityIntervalSpellingRule which) {
    return which >= 0 && which < STANDARD_TONALITY_INTERVAL_SPELLING_RULE_COUNT;
}

const Rule *standard_tonality_interval_spelling_rule(StandardTonalityIntervalSpellingRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_interval_spelling_constraint(StandardTonalityIntervalSpellingRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_tonality_spell_above(const PitchSpelling *lower, int size, StandardTonalityIntervalQuality quality,
                                   PitchSpelling **out) {
    if (lower == NULL || out == NULL || size < 1 || !standard_tonality_interval_quality_fits_size(size, quality)) {
        return false;
    }

    int semitones;
    if (!standard_tonality_interval_semitones(size, quality, &semitones)) {
        return false;
    }

    /* The letter size - 1 letters above, and the octave number it falls
       in (the octave number belongs to the letter). */
    int lower_place = pitch_letter_index(pitch_spelling_letter(lower));
    int upper_place = lower_place + size - 1;
    PitchLetter letter = pitch_spelling_letter(lower);
    for (int i = 1; i < size; i++) {
        letter = standard_tonality_next_letter(letter);
    }
    int octave = pitch_spelling_octave(lower) + upper_place / 7;

    int target = standard_tonality_semitone_number(lower) + semitones;
    int accidental = target - (STANDARD_TONALITY_SEMITONES_PER_OCTAVE * octave + standard_tonality_letter_semitone(letter));
    if (accidental < -2 || accidental > 2) {
        return false;
    }

    return pitch_spelling_create(letter, accidental, octave, out) == PITCH_SPELLING_OK;
}

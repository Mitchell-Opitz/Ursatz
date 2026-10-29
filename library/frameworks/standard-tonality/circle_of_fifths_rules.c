#include "circle_of_fifths_rules.h"

#include <stddef.h>

#include "minor_key_rules.h"
#include "rule_pair.h"
#include "pitch/pitch_arithmetic.h"
#include "tonal_key.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_CIRCLE_RULE_COUNT] = {
    "circle-of-fifths-order", "circle-of-fifths-neighbors", "circle-of-fifths-enharmonic",
    "circle-of-fifths-real-imaginary", "circle-of-fifths-minor-ring"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_CIRCLE_RULE_COUNT);

static bool valid_rule(StandardTonalityCircleRule which) {
    return which >= 0 && which < STANDARD_TONALITY_CIRCLE_RULE_COUNT;
}

const Rule *standard_tonality_circle_rule(StandardTonalityCircleRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_circle_constraint(StandardTonalityCircleRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

#define MAX_ACCIDENTALS 7

int standard_tonality_major_key_accidentals(PitchLetter letter, int accidental) {
    int fifths = 0;
    standard_tonality_key_fifths(letter, accidental, false, &fifths);
    return fifths;
}

bool standard_tonality_major_key_is_real(PitchLetter letter, int accidental) {
    int count = standard_tonality_major_key_accidentals(letter, accidental);
    return count >= -MAX_ACCIDENTALS && count <= MAX_ACCIDENTALS;
}

bool standard_tonality_circle_neighbor(PitchLetter letter, int accidental, bool clockwise, PitchLetter *next_letter,
                                       int *next_accidental) {
    return standard_tonality_circle_step(letter, accidental, clockwise ? 1 : -1, next_letter, next_accidental);
}

bool standard_tonality_major_keys_enharmonic(PitchLetter letter_a, int accidental_a, PitchLetter letter_b,
                                             int accidental_b) {
    if (!standard_tonality_major_key_is_real(letter_a, accidental_a) ||
        !standard_tonality_major_key_is_real(letter_b, accidental_b)) {
        return false;
    }
    if (letter_a == letter_b && accidental_a == accidental_b) {
        return false;
    }
    return pitch_class_of_letter(letter_a, accidental_a) == pitch_class_of_letter(letter_b, accidental_b);
}

bool standard_tonality_minor_keys_enharmonic(PitchLetter letter_a, int accidental_a, PitchLetter letter_b,
                                             int accidental_b) {
    if (!standard_tonality_minor_key_is_real(letter_a, accidental_a) ||
        !standard_tonality_minor_key_is_real(letter_b, accidental_b)) {
        return false;
    }
    if (letter_a == letter_b && accidental_a == accidental_b) {
        return false;
    }
    return pitch_class_of_letter(letter_a, accidental_a) == pitch_class_of_letter(letter_b, accidental_b);
}

bool standard_tonality_major_key_signature(PitchLetter letter, int accidental, KeySignature **out) {
    return standard_tonality_key_signature_of(letter, accidental, false, out);
}

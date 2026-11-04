#include "step_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "pitch/pitch_arithmetic.h"
#include "semitone.h"

static const char *const NAMES[STANDARD_NOTATION_STEP_RULE_COUNT] = {"half-step", "whole-step",
                                                                      "natural-half-steps"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_STEP_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_notation_step, StandardNotationStepRule, STANDARD_NOTATION_STEP_RULE_COUNT, g_table)

static int absolute_distance(const PitchSpelling *a, const PitchSpelling *b) {
    int distance = standard_notation_semitone_distance(a, b);
    return distance < 0 ? -distance : distance;
}

bool standard_notation_is_half_step(const PitchSpelling *a, const PitchSpelling *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return absolute_distance(a, b) == 1;
}

bool standard_notation_is_whole_step(const PitchSpelling *a, const PitchSpelling *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return absolute_distance(a, b) == 2;
}

int standard_notation_half_steps_per_whole_step(void) {
    return 2;
}

int standard_notation_semitone_above(const PitchSpelling *spelling, int half_steps) {
    return standard_notation_semitone_number(spelling) + half_steps;
}

PitchLetter standard_notation_next_letter(PitchLetter letter) {
    return pitch_letter_offset(letter, 1);
}

int standard_notation_natural_step_size(PitchLetter letter) {
    int size = standard_notation_letter_semitone(standard_notation_next_letter(letter)) -
               standard_notation_letter_semitone(letter);
    if (size < 0) {
        size += STANDARD_NOTATION_SEMITONES_PER_OCTAVE;
    }
    return size;
}

bool standard_notation_natural_half_step_above(PitchLetter letter) {
    return standard_notation_natural_step_size(letter) == 1;
}

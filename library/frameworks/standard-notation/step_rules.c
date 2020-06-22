#include "step_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "semitone.h"

static const char *const NAMES[STANDARD_NOTATION_STEP_RULE_COUNT] = {"half-step", "whole-step",
                                                                      "natural-half-steps"};

static StandardNotationRulePair g_pairs[STANDARD_NOTATION_STEP_RULE_COUNT];
static bool g_built[STANDARD_NOTATION_STEP_RULE_COUNT];

static bool valid_rule(StandardNotationStepRule which) {
    return which >= 0 && which < STANDARD_NOTATION_STEP_RULE_COUNT;
}

static void ensure_built(StandardNotationStepRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_notation_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_notation_step_rule(StandardNotationStepRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_notation_step_constraint(StandardNotationStepRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

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
    switch (letter) {
        case PITCH_LETTER_C:
            return PITCH_LETTER_D;
        case PITCH_LETTER_D:
            return PITCH_LETTER_E;
        case PITCH_LETTER_E:
            return PITCH_LETTER_F;
        case PITCH_LETTER_F:
            return PITCH_LETTER_G;
        case PITCH_LETTER_G:
            return PITCH_LETTER_A;
        case PITCH_LETTER_A:
            return PITCH_LETTER_B;
        case PITCH_LETTER_B:
        default:
            return PITCH_LETTER_C;
    }
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

#include "modal_solfege_rules.h"

#include <stddef.h>

#include "major_scale_rules.h"
#include "rule_pair.h"
#include "solfege_rules.h"

static const char *const NAMES[STANDARD_TONALITY_MODAL_SOLFEGE_RULE_COUNT] = {"modal-solfege-syllables"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_MODAL_SOLFEGE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_modal_solfege, StandardTonalityModalSolfegeRule, STANDARD_TONALITY_MODAL_SOLFEGE_RULE_COUNT, g_table)

/* Semitones above the tonic of degree 1..7: the sum of the first
   degree - 1 steps. */
static int mode_position(StandardTonalityMode mode, int degree) {
    int position = 0;
    for (int i = 0; i < degree - 1; i++) {
        position += standard_tonality_mode_step(mode, (size_t)i);
    }
    return position;
}

static int major_position(int degree) {
    int position = 0;
    for (int i = 0; i < degree - 1; i++) {
        position += standard_tonality_major_scale_step((size_t)i);
    }
    return position;
}

int standard_tonality_mode_degree_alteration(StandardTonalityMode mode, int degree) {
    if (standard_tonality_mode_name(mode) == NULL || degree < 1 || degree > 7) {
        return 0;
    }
    return mode_position(mode, degree) - major_position(degree);
}

const char *standard_tonality_mode_solfege(StandardTonalityMode mode, int degree) {
    if (standard_tonality_mode_name(mode) == NULL || degree < 1 || degree > 8) {
        return NULL;
    }
    if (degree == 8) {
        return standard_tonality_chromatic_solfege(1, 0);
    }
    return standard_tonality_chromatic_solfege(degree, standard_tonality_mode_degree_alteration(mode, degree));
}

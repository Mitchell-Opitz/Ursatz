#include "octave_equivalence_rules.h"

#include "pitch/clef_resolution.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_OCTAVE_RULE_COUNT] = {"octave-equivalence"};

static StandardNotationRulePair g_pairs[STANDARD_NOTATION_OCTAVE_RULE_COUNT];
static bool g_built[STANDARD_NOTATION_OCTAVE_RULE_COUNT];

static bool valid_rule(StandardNotationOctaveRule which) {
    return which >= 0 && which < STANDARD_NOTATION_OCTAVE_RULE_COUNT;
}

static void ensure_built(StandardNotationOctaveRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_notation_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_notation_octave_rule(StandardNotationOctaveRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_notation_octave_constraint(StandardNotationOctaveRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

int standard_notation_letter_count(void) {
    return 7;
}

bool standard_notation_octave_equivalent(PitchLetter a, PitchLetter b) {
    return a == b;
}

bool standard_notation_positions_octave_equivalent(const Clef *clef, const StaffPosition *a,
                                                   const StaffPosition *b) {
    if (clef == NULL || a == NULL || b == NULL) {
        return false;
    }
    PitchLetter letter_a, letter_b;
    int octave;
    if (clef_pitch_at_position(clef, a, &letter_a, &octave) != CLEF_RESOLUTION_OK ||
        clef_pitch_at_position(clef, b, &letter_b, &octave) != CLEF_RESOLUTION_OK) {
        return false;
    }
    return standard_notation_octave_equivalent(letter_a, letter_b);
}

#include "octave_equivalence_rules.h"

#include "pitch/clef_resolution.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_OCTAVE_RULE_COUNT] = {"octave-equivalence"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_OCTAVE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_notation_octave, StandardNotationOctaveRule, STANDARD_NOTATION_OCTAVE_RULE_COUNT, g_table)

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

#include "staff_clef_rules.h"

#include "pitch/clef_resolution.h"
#include "pitch/pitch_spelling.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_STAFF_RULE_COUNT] = {"staff-five-lines", "clef-reference",
                                                                       "pitch-height"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_STAFF_RULE_COUNT);

static bool valid_rule(StandardNotationStaffRule which) {
    return which >= 0 && which < STANDARD_NOTATION_STAFF_RULE_COUNT;
}

const Rule *standard_notation_staff_rule(StandardNotationStaffRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_notation_staff_constraint(StandardNotationStaffRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

int standard_notation_staff_line_count(void) {
    return STAFF_POSITION_LINE_COUNT;
}

bool standard_notation_clef_places_reference(const Clef *clef, const StaffPosition *position) {
    if (clef == NULL || position == NULL) {
        return false;
    }
    int reference_steps;
    if (clef_reference_steps(clef, &reference_steps) != CLEF_RESOLUTION_OK) {
        return false;
    }
    return staff_position_steps(position) == reference_steps;
}

/* Absolute diatonic number (octave * 7 + letter index, C = 0 ... B = 6) of
   the pitch written at position under clef. */
static bool written_rank(const Clef *clef, const StaffPosition *position, int *out_rank) {
    static const int LETTER_INDEX[] = {5, 6, 0, 1, 2, 3, 4}; /* indexed by PitchLetter A..G */

    PitchLetter letter;
    int octave;
    if (clef_pitch_at_position(clef, position, &letter, &octave) != CLEF_RESOLUTION_OK) {
        return false;
    }
    *out_rank = octave * 7 + LETTER_INDEX[letter];
    return true;
}

bool standard_notation_pitch_height_respected(const Clef *clef, const StaffPosition *lower,
                                              const StaffPosition *higher) {
    if (clef == NULL || lower == NULL || higher == NULL) {
        return false;
    }
    int lower_steps = staff_position_steps(lower);
    int higher_steps = staff_position_steps(higher);
    int lower_rank, higher_rank;
    if (!written_rank(clef, lower, &lower_rank) || !written_rank(clef, higher, &higher_rank)) {
        return false; /* unpitched clef: no pitch order to respect */
    }

    if (higher_steps > lower_steps) {
        return higher_rank > lower_rank;
    }
    if (higher_steps == lower_steps) {
        return higher_rank == lower_rank;
    }
    return higher_rank < lower_rank;
}

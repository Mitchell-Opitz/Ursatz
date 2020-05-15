#include "staff_clef_rules.h"

#include "pitch/clef_resolution.h"
#include "pitch/pitch_spelling.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_STAFF_RULE_COUNT] = {"staff-five-lines", "clef-reference",
                                                                       "pitch-height"};

static StandardNotationRulePair g_pairs[STANDARD_NOTATION_STAFF_RULE_COUNT];
static bool g_built[STANDARD_NOTATION_STAFF_RULE_COUNT];

static bool valid_rule(StandardNotationStaffRule which) {
    return which >= 0 && which < STANDARD_NOTATION_STAFF_RULE_COUNT;
}

static void ensure_built(StandardNotationStaffRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_notation_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_notation_staff_rule(StandardNotationStaffRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_notation_staff_constraint(StandardNotationStaffRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

int standard_notation_staff_line_count(void) {
    return STAFF_POSITION_LINE_COUNT;
}

bool standard_notation_clef_places_reference(const Clef *clef, const StaffPosition *position) {
    if (clef == NULL || position == NULL) {
        return false;
    }
    return staff_position_steps(position) == clef_reference_steps(clef);
}

/* Absolute diatonic number (octave * 7 + letter index, C = 0 ... B = 6) of
   the pitch written at position under clef. */
static int written_rank(const Clef *clef, const StaffPosition *position) {
    static const int LETTER_INDEX[] = {5, 6, 0, 1, 2, 3, 4}; /* indexed by PitchLetter A..G */

    PitchLetter letter;
    int octave;
    clef_pitch_at_position(clef, position, &letter, &octave);
    return octave * 7 + LETTER_INDEX[letter];
}

bool standard_notation_pitch_height_respected(const Clef *clef, const StaffPosition *lower,
                                              const StaffPosition *higher) {
    if (clef == NULL || lower == NULL || higher == NULL) {
        return false;
    }
    int lower_steps = staff_position_steps(lower);
    int higher_steps = staff_position_steps(higher);
    int lower_rank = written_rank(clef, lower);
    int higher_rank = written_rank(clef, higher);

    if (higher_steps > lower_steps) {
        return higher_rank > lower_rank;
    }
    if (higher_steps == lower_steps) {
        return higher_rank == lower_rank;
    }
    return higher_rank < lower_rank;
}

#include "clef_letter_rules.h"

#include "pitch/clef_resolution.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_CLEF_LETTER_RULE_COUNT] = {"clef-line-space-letters",
                                                                             "clef-ledger-continuation"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_CLEF_LETTER_RULE_COUNT);

static bool valid_rule(StandardNotationClefLetterRule which) {
    return which >= 0 && which < STANDARD_NOTATION_CLEF_LETTER_RULE_COUNT;
}

const Rule *standard_notation_clef_letter_rule(StandardNotationClefLetterRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_notation_clef_letter_constraint(StandardNotationClefLetterRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

/* Letters in ascending order within an octave, C first. PitchLetter itself
   is ordered A..G, so stepping needs its own cycle. */
static const PitchLetter CYCLE[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                      PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};

static int cycle_index(PitchLetter letter) {
    for (int i = 0; i < 7; i++) {
        if (CYCLE[i] == letter) {
            return i;
        }
    }
    return 0;
}

static bool letter_at_steps(const Clef *clef, int steps, PitchLetter *out) {
    StaffPosition *position = NULL;
    if (staff_position_create(steps, &position) != STAFF_POSITION_OK) {
        return false;
    }
    int octave;
    bool ok = clef_pitch_at_position(clef, position, out, &octave) == CLEF_RESOLUTION_OK;
    staff_position_destroy(position);
    return ok;
}

bool standard_notation_clef_line_letter(const Clef *clef, int line, PitchLetter *out) {
    if (clef == NULL || line < 1 || line > STANDARD_NOTATION_STAFF_LINES) {
        return false;
    }
    return letter_at_steps(clef, 2 * (line - 1), out);
}

bool standard_notation_clef_space_letter(const Clef *clef, int space, PitchLetter *out) {
    if (clef == NULL || space < 1 || space > STANDARD_NOTATION_STAFF_SPACES) {
        return false;
    }
    return letter_at_steps(clef, 2 * space - 1, out);
}

bool standard_notation_ledger_continues_letters(const Clef *clef, const StaffPosition *position) {
    if (clef == NULL || position == NULL || !clef_is_pitched(clef)) {
        return false;
    }

    int steps = staff_position_steps(position);
    int edge = steps > STAFF_POSITION_TOP_LINE ? STAFF_POSITION_TOP_LINE : STAFF_POSITION_BOTTOM_LINE;
    if (steps == edge || staff_position_is_on_staff(position)) {
        return true;
    }

    PitchLetter edge_letter, actual;
    if (!letter_at_steps(clef, edge, &edge_letter) || !letter_at_steps(clef, steps, &actual)) {
        return false;
    }

    int expected_index = (cycle_index(edge_letter) + (steps - edge)) % 7;
    if (expected_index < 0) {
        expected_index += 7;
    }
    return actual == CYCLE[expected_index];
}

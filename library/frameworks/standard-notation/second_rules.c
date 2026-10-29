#include "second_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_SECOND_RULE_COUNT] = {"second-lower-note-left"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_SECOND_RULE_COUNT);

static bool valid_rule(StandardNotationSecondRule which) {
    return which >= 0 && which < STANDARD_NOTATION_SECOND_RULE_COUNT;
}

const Rule *standard_notation_second_rule(StandardNotationSecondRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_notation_second_constraint(StandardNotationSecondRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_notation_is_second(const StaffPosition *a, const StaffPosition *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    int difference = staff_position_steps(a) - staff_position_steps(b);
    return difference == 1 || difference == -1;
}

bool standard_notation_second_displacement_valid(const StaffPosition *lower, NoteheadSide lower_side,
                                                 const StaffPosition *upper, NoteheadSide upper_side) {
    if (lower == NULL || upper == NULL) {
        return false;
    }
    if (!standard_notation_is_second(lower, upper)) {
        return true;
    }
    if (staff_position_steps(lower) > staff_position_steps(upper)) {
        return false; /* arguments swapped: "lower" is not the lower note */
    }
    return lower_side == NOTEHEAD_SIDE_LEFT && upper_side == NOTEHEAD_SIDE_RIGHT;
}

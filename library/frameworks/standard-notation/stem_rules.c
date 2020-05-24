#include "stem_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_STEM_RULE_COUNT] = {"stem-above-middle-down",
                                                                      "stem-below-middle-up",
                                                                      "stem-on-middle-either"};

static StandardNotationRulePair g_pairs[STANDARD_NOTATION_STEM_RULE_COUNT];
static bool g_built[STANDARD_NOTATION_STEM_RULE_COUNT];

static bool valid_rule(StandardNotationStemRule which) {
    return which >= 0 && which < STANDARD_NOTATION_STEM_RULE_COUNT;
}

static void ensure_built(StandardNotationStemRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_notation_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_notation_stem_rule(StandardNotationStemRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_notation_stem_constraint(StandardNotationStemRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

int standard_notation_stem_direction_option_count(const StaffPosition *position) {
    if (position == NULL) {
        return 0;
    }
    return staff_position_steps(position) == STAFF_POSITION_MIDDLE_LINE ? 2 : 1;
}

StemDirection standard_notation_stem_direction_default(const StaffPosition *position) {
    if (position != NULL && staff_position_steps(position) < STAFF_POSITION_MIDDLE_LINE) {
        return STEM_DIRECTION_UP;
    }
    return STEM_DIRECTION_DOWN;
}

bool standard_notation_stem_direction_valid(const StaffPosition *position, StemDirection direction) {
    if (position == NULL) {
        return false;
    }
    int steps = staff_position_steps(position);
    if (steps == STAFF_POSITION_MIDDLE_LINE) {
        return true;
    }
    return direction == (steps > STAFF_POSITION_MIDDLE_LINE ? STEM_DIRECTION_DOWN : STEM_DIRECTION_UP);
}

#include "flag_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_FLAG_RULE_COUNT] = {"flag-down-stem-inward", "flag-up-stem-outward",
                                                                      "flag-middle-either"};

static StandardNotationRulePair g_pairs[STANDARD_NOTATION_FLAG_RULE_COUNT];
static bool g_built[STANDARD_NOTATION_FLAG_RULE_COUNT];

static bool valid_rule(StandardNotationFlagRule which) {
    return which >= 0 && which < STANDARD_NOTATION_FLAG_RULE_COUNT;
}

static void ensure_built(StandardNotationFlagRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_notation_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_notation_flag_rule(StandardNotationFlagRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_notation_flag_constraint(StandardNotationFlagRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

FlagFacing standard_notation_flag_facing(StemDirection stem_direction) {
    return stem_direction == STEM_DIRECTION_DOWN ? FLAG_FACING_INWARD : FLAG_FACING_OUTWARD;
}

int standard_notation_flag_facing_option_count(const StaffPosition *position) {
    return standard_notation_stem_direction_option_count(position);
}

FlagFacing standard_notation_flag_facing_default(const StaffPosition *position) {
    return standard_notation_flag_facing(standard_notation_stem_direction_default(position));
}

bool standard_notation_flag_facing_valid(const StaffPosition *position, FlagFacing facing) {
    if (position == NULL) {
        return false;
    }
    StemDirection stem = facing == FLAG_FACING_INWARD ? STEM_DIRECTION_DOWN : STEM_DIRECTION_UP;
    return standard_notation_stem_direction_valid(position, stem);
}

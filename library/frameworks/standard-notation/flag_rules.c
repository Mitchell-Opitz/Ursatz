#include "flag_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_FLAG_RULE_COUNT] = {"flag-down-stem-inward", "flag-up-stem-outward",
                                                                      "flag-middle-either"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_FLAG_RULE_COUNT);

static bool valid_rule(StandardNotationFlagRule which) {
    return which >= 0 && which < STANDARD_NOTATION_FLAG_RULE_COUNT;
}

const Rule *standard_notation_flag_rule(StandardNotationFlagRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_notation_flag_constraint(StandardNotationFlagRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
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

#include "flag_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_FLAG_RULE_COUNT] = {"flag-down-stem-inward", "flag-up-stem-outward",
                                                                      "flag-middle-either"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_FLAG_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_notation_flag, StandardNotationFlagRule, STANDARD_NOTATION_FLAG_RULE_COUNT, g_table)

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

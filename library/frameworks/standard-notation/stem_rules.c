#include "stem_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_STEM_RULE_COUNT] = {"stem-above-middle-down",
                                                                      "stem-below-middle-up",
                                                                      "stem-on-middle-either"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_STEM_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_notation_stem, StandardNotationStemRule, STANDARD_NOTATION_STEM_RULE_COUNT, g_table)

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

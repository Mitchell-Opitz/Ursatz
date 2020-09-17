#include <assert.h>
#include <stdio.h>

#include "flag_rules.h"
#include "framework.h"

static StaffPosition *position(int steps) {
    StaffPosition *p = NULL;
    assert(staff_position_create(steps, &p) == STAFF_POSITION_OK);
    return p;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_FLAG_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_flag_rule((StandardNotationFlagRule)i);
        const Constraint *constraint = standard_notation_flag_constraint((StandardNotationFlagRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_notation_flag_rule(STANDARD_NOTATION_FLAG_RULE_COUNT) == NULL);
}

/* Down-stem goes with an inward flag, up-stem with an outward one. */
static void test_facing_follows_stem(void) {
    assert(standard_notation_flag_facing(STEM_DIRECTION_DOWN) == FLAG_FACING_INWARD);
    assert(standard_notation_flag_facing(STEM_DIRECTION_UP) == FLAG_FACING_OUTWARD);
}

static void test_above_middle_inward(void) {
    for (int steps = 5; steps <= 12; steps++) {
        StaffPosition *p = position(steps);
        assert(standard_notation_flag_facing_option_count(p) == 1);
        assert(standard_notation_flag_facing_default(p) == FLAG_FACING_INWARD);
        assert(standard_notation_flag_facing_valid(p, FLAG_FACING_INWARD));
        assert(!standard_notation_flag_facing_valid(p, FLAG_FACING_OUTWARD));
        staff_position_destroy(p);
    }
}

static void test_below_middle_outward(void) {
    for (int steps = -4; steps <= 3; steps++) {
        StaffPosition *p = position(steps);
        assert(standard_notation_flag_facing_option_count(p) == 1);
        assert(standard_notation_flag_facing_default(p) == FLAG_FACING_OUTWARD);
        assert(standard_notation_flag_facing_valid(p, FLAG_FACING_OUTWARD));
        assert(!standard_notation_flag_facing_valid(p, FLAG_FACING_INWARD));
        staff_position_destroy(p);
    }
}

/* On the middle line the flag may go either way. */
static void test_middle_line_either(void) {
    StaffPosition *p = position(STAFF_POSITION_MIDDLE_LINE);
    assert(standard_notation_flag_facing_option_count(p) == 2);
    assert(standard_notation_flag_facing_valid(p, FLAG_FACING_INWARD));
    assert(standard_notation_flag_facing_valid(p, FLAG_FACING_OUTWARD));
    /* default follows the single-note stem default (down) */
    assert(standard_notation_flag_facing_default(p) == FLAG_FACING_INWARD);
    staff_position_destroy(p);
}

static void test_null_position(void) {
    assert(standard_notation_flag_facing_option_count(NULL) == 0);
    assert(!standard_notation_flag_facing_valid(NULL, FLAG_FACING_INWARD));
}

int main(void) {
    test_rules_reference_framework();
    test_facing_follows_stem();
    test_above_middle_inward();
    test_below_middle_outward();
    test_middle_line_either();
    test_null_position();
    printf("test_flag_rules (standard-notation): ok\n");
    return 0;
}

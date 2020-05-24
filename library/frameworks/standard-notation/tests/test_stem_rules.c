#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "stem_rules.h"

static StaffPosition *position(int steps) {
    StaffPosition *p = NULL;
    assert(staff_position_create(steps, &p) == STAFF_POSITION_OK);
    return p;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_STEM_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_stem_rule((StandardNotationStemRule)i);
        const Constraint *constraint = standard_notation_stem_constraint((StandardNotationStemRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_notation_stem_rule(STANDARD_NOTATION_STEM_RULE_COUNT) == NULL);
}

static void test_above_middle_points_down(void) {
    for (int steps = 5; steps <= 12; steps++) {
        StaffPosition *p = position(steps);
        assert(standard_notation_stem_direction_option_count(p) == 1);
        assert(standard_notation_stem_direction_default(p) == STEM_DIRECTION_DOWN);
        assert(standard_notation_stem_direction_valid(p, STEM_DIRECTION_DOWN));
        assert(!standard_notation_stem_direction_valid(p, STEM_DIRECTION_UP));
        staff_position_destroy(p);
    }
}

static void test_below_middle_points_up(void) {
    for (int steps = -4; steps <= 3; steps++) {
        StaffPosition *p = position(steps);
        assert(standard_notation_stem_direction_option_count(p) == 1);
        assert(standard_notation_stem_direction_default(p) == STEM_DIRECTION_UP);
        assert(standard_notation_stem_direction_valid(p, STEM_DIRECTION_UP));
        assert(!standard_notation_stem_direction_valid(p, STEM_DIRECTION_DOWN));
        staff_position_destroy(p);
    }
}

static void test_middle_line_either(void) {
    StaffPosition *p = position(4);
    assert(standard_notation_stem_direction_option_count(p) == 2);
    assert(standard_notation_stem_direction_valid(p, STEM_DIRECTION_UP));
    assert(standard_notation_stem_direction_valid(p, STEM_DIRECTION_DOWN));
    staff_position_destroy(p);

    assert(standard_notation_stem_direction_option_count(NULL) == 0);
    assert(!standard_notation_stem_direction_valid(NULL, STEM_DIRECTION_UP));
}

int main(void) {
    test_rules_reference_framework();
    test_above_middle_points_down();
    test_below_middle_points_up();
    test_middle_line_either();
    printf("test_stem_rules (standard-notation): ok\n");
    return 0;
}

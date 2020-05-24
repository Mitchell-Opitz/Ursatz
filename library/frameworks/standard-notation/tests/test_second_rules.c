#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "second_rules.h"

static StaffPosition *position(int steps) {
    StaffPosition *p = NULL;
    assert(staff_position_create(steps, &p) == STAFF_POSITION_OK);
    return p;
}

static void test_rules_reference_framework(void) {
    const Rule *rule = standard_notation_second_rule(STANDARD_NOTATION_SECOND_RULE_LOWER_NOTE_LEFT);
    const Constraint *constraint =
        standard_notation_second_constraint(STANDARD_NOTATION_SECOND_RULE_LOWER_NOTE_LEFT);
    assert(rule != NULL && constraint != NULL);
    assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
    assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    assert(standard_notation_second_rule(STANDARD_NOTATION_SECOND_RULE_COUNT) == NULL);
}

static void test_is_second(void) {
    StaffPosition *a = position(3), *b = position(4), *c = position(5);
    assert(standard_notation_is_second(a, b));
    assert(standard_notation_is_second(b, a));
    assert(!standard_notation_is_second(a, c));
    assert(!standard_notation_is_second(a, a));
    assert(!standard_notation_is_second(a, NULL));
    staff_position_destroy(a);
    staff_position_destroy(b);
    staff_position_destroy(c);
}

/* Lower note left, upper note right: correct whichever way the stem points,
   including across line/space and space/line seconds. */
static void test_lower_left_is_valid(void) {
    for (int lower_steps = -2; lower_steps <= 9; lower_steps++) {
        StaffPosition *lower = position(lower_steps), *upper = position(lower_steps + 1);
        assert(standard_notation_second_displacement_valid(lower, NOTEHEAD_SIDE_LEFT, upper, NOTEHEAD_SIDE_RIGHT));
        staff_position_destroy(lower);
        staff_position_destroy(upper);
    }
}

static void test_incorrect_seconds(void) {
    StaffPosition *lower = position(2), *upper = position(3);

    /* Lower note on the right. */
    assert(!standard_notation_second_displacement_valid(lower, NOTEHEAD_SIDE_RIGHT, upper, NOTEHEAD_SIDE_LEFT));
    /* Stacked: neither note displaced. */
    assert(!standard_notation_second_displacement_valid(lower, NOTEHEAD_SIDE_CENTER, upper, NOTEHEAD_SIDE_CENTER));
    /* Both on the same side. */
    assert(!standard_notation_second_displacement_valid(lower, NOTEHEAD_SIDE_LEFT, upper, NOTEHEAD_SIDE_LEFT));
    assert(!standard_notation_second_displacement_valid(lower, NOTEHEAD_SIDE_RIGHT, upper, NOTEHEAD_SIDE_RIGHT));
    /* Swapped arguments: "lower" is actually the upper note. */
    assert(!standard_notation_second_displacement_valid(upper, NOTEHEAD_SIDE_LEFT, lower, NOTEHEAD_SIDE_RIGHT));

    staff_position_destroy(lower);
    staff_position_destroy(upper);
}

/* Non-seconds are outside this rule: any placement is acceptable here. */
static void test_non_second_unconstrained(void) {
    StaffPosition *lower = position(2), *upper = position(4);
    assert(standard_notation_second_displacement_valid(lower, NOTEHEAD_SIDE_CENTER, upper, NOTEHEAD_SIDE_CENTER));
    staff_position_destroy(lower);
    staff_position_destroy(upper);

    assert(!standard_notation_second_displacement_valid(NULL, NOTEHEAD_SIDE_LEFT, NULL, NOTEHEAD_SIDE_RIGHT));
}

int main(void) {
    test_rules_reference_framework();
    test_is_second();
    test_lower_left_is_valid();
    test_incorrect_seconds();
    test_non_second_unconstrained();
    printf("test_second_rules (standard-notation): ok\n");
    return 0;
}

#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "ledger_line_rules.h"

static StaffPosition *position(int steps) {
    StaffPosition *p = NULL;
    assert(staff_position_create(steps, &p) == STAFF_POSITION_OK);
    return p;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_LEDGER_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_ledger_rule((StandardNotationLedgerRule)i);
        const Constraint *constraint = standard_notation_ledger_constraint((StandardNotationLedgerRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_notation_ledger_rule(STANDARD_NOTATION_LEDGER_RULE_COUNT) == NULL);
}

static void test_required_counts(void) {
    int steps[] = {-5, -4, -3, -2, -1, 0, 4, 8, 9, 10, 11, 12, 13};
    int expected[] = {2, 2, 1, 1, 0, 0, 0, 0, 0, 1, 1, 2, 2};
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        StaffPosition *p = position(steps[i]);
        assert(standard_notation_ledger_lines_required(p) == expected[i]);
        staff_position_destroy(p);
    }
    assert(standard_notation_ledger_lines_required(NULL) == 0);
}

/* A note takes exactly the lines between it and the staff: fewer or extra
   ledger lines are both invalid. */
static void test_no_extra_ledger_lines(void) {
    StaffPosition *first_above = position(10);
    assert(standard_notation_ledger_lines_valid(first_above, 1));
    assert(!standard_notation_ledger_lines_valid(first_above, 2));
    assert(!standard_notation_ledger_lines_valid(first_above, 0));
    staff_position_destroy(first_above);

    StaffPosition *space_above_first = position(11);
    assert(standard_notation_ledger_lines_valid(space_above_first, 1));
    assert(!standard_notation_ledger_lines_valid(space_above_first, 2));
    staff_position_destroy(space_above_first);

    StaffPosition *on_staff = position(4);
    assert(standard_notation_ledger_lines_valid(on_staff, 0));
    assert(!standard_notation_ledger_lines_valid(on_staff, 1));
    staff_position_destroy(on_staff);

    assert(!standard_notation_ledger_lines_valid(NULL, 0));
}

int main(void) {
    test_rules_reference_framework();
    test_required_counts();
    test_no_extra_ledger_lines();
    printf("test_ledger_line_rules (standard-notation): ok\n");
    return 0;
}

#include "pitch/staff_position.h"

#include <assert.h>
#include <stddef.h>

static int ledger_count(int steps) {
    StaffPosition *position = NULL;
    assert(staff_position_create(steps, &position) == STAFF_POSITION_OK);
    int count = staff_position_ledger_line_count(position);
    staff_position_destroy(position);
    return count;
}

static void test_lines_and_spaces(void) {
    StaffPosition *line = NULL, *space = NULL;
    assert(staff_position_create(0, &line) == STAFF_POSITION_OK);
    assert(staff_position_create(1, &space) == STAFF_POSITION_OK);
    assert(staff_position_is_line(line) && !staff_position_is_space(line));
    assert(staff_position_is_space(space) && !staff_position_is_line(space));
    staff_position_destroy(line);
    staff_position_destroy(space);

    /* Negative positions keep the same parity. */
    StaffPosition *below = NULL;
    assert(staff_position_create(-2, &below) == STAFF_POSITION_OK);
    assert(staff_position_is_line(below));
    staff_position_destroy(below);
}

static void test_on_staff(void) {
    for (int steps = -3; steps <= 11; steps++) {
        StaffPosition *position = NULL;
        assert(staff_position_create(steps, &position) == STAFF_POSITION_OK);
        assert(staff_position_is_on_staff(position) == (steps >= 0 && steps <= 8));
        staff_position_destroy(position);
    }
}

static void test_ledger_line_counts(void) {
    for (int steps = 0; steps <= 9; steps++) {
        assert(ledger_count(steps) == 0);
    }
    assert(ledger_count(-1) == 0);
    assert(ledger_count(10) == 1);
    assert(ledger_count(11) == 1);
    assert(ledger_count(12) == 2);
    assert(ledger_count(13) == 2);
    assert(ledger_count(-2) == 1);
    assert(ledger_count(-3) == 1);
    assert(ledger_count(-4) == 2);
    assert(ledger_count(-5) == 2);
}

static void test_equality(void) {
    StaffPosition *a = NULL, *b = NULL, *c = NULL;
    assert(staff_position_create(4, &a) == STAFF_POSITION_OK);
    assert(staff_position_create(4, &b) == STAFF_POSITION_OK);
    assert(staff_position_create(5, &c) == STAFF_POSITION_OK);
    assert(staff_position_equals(a, b));
    assert(!staff_position_equals(a, c));
    assert(!staff_position_equals(a, NULL));
    staff_position_destroy(a);
    staff_position_destroy(b);
    staff_position_destroy(c);
}

int main(void) {
    test_lines_and_spaces();
    test_on_staff();
    test_ledger_line_counts();
    test_equality();
    return 0;
}

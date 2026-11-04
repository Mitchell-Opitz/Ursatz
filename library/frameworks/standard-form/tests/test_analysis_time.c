#include <assert.h>
#include <stdio.h>

#include "analysis_time.h"

/*
 * The standard-form time module is a wrapper over the kernel's checked score time. Ordinary values read as before; a value
 * that does not fit a Rational is refused and reads as 0/1, its failure value. Two coprime denominators near 2^62 make every
 * cross product overflow.
 */

#define BIG 4000000000000000000LL

static bool is_zero(StandardFormQ q) {
    return q.n == 0 && q.d == 1;
}

static void test_ordinary_values(void) {
    StandardFormQ a = standard_form_q(1, 4);
    StandardFormQ b = standard_form_q(1, 6);
    StandardFormQ sum = standard_form_q_add(a, b);
    assert(sum.n == 5 && sum.d == 12);
    StandardFormQ difference = standard_form_q_sub(a, b);
    assert(difference.n == 1 && difference.d == 12);
}

static void test_add_overflow_is_refused(void) {
    assert(is_zero(standard_form_q_add(standard_form_q(1, BIG), standard_form_q(1, BIG + 1))));
}

static void test_sub_overflow_is_refused(void) {
    assert(is_zero(standard_form_q_sub(standard_form_q(1, BIG), standard_form_q(1, BIG + 1))));
}

int main(void) {
    test_ordinary_values();
    test_add_overflow_is_refused();
    test_sub_overflow_is_refused();
    printf("standard_form_analysis_time: ok\n");
    return 0;
}

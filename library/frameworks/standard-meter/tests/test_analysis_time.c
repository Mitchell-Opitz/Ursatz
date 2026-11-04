#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "meter_analysis_time.h"

/*
 * The standard-meter time module is a wrapper over the kernel's checked score time. Ordinary values read as before; a value
 * that does not fit a Rational is refused: the operators return 0/1 (their failure value) and the snap leaves the time alone.
 * Two coprime denominators near 2^62 make every cross product overflow.
 */

#define BIG 4000000000000000000LL

static bool is_zero(StandardMeterQ q) {
    return q.n == 0 && q.d == 1;
}

static void test_ordinary_values(void) {
    StandardMeterQ a = standard_meter_q(1, 4);
    StandardMeterQ b = standard_meter_q(1, 6);
    StandardMeterQ sum = standard_meter_q_add(a, b);
    assert(sum.n == 5 && sum.d == 12);
    StandardMeterQ difference = standard_meter_q_sub(a, b);
    assert(difference.n == 1 && difference.d == 12);
    StandardMeterQ product = standard_meter_q_mul_int(a, 6);
    assert(product.n == 3 && product.d == 2);
    StandardMeterQ quotient = standard_meter_q_div_int(a, 2);
    assert(quotient.n == 1 && quotient.d == 8);
    assert(standard_meter_q(2, -4).n == -1 && standard_meter_q(2, -4).d == 2);
}

static void test_add_overflow_is_refused(void) {
    assert(is_zero(standard_meter_q_add(standard_meter_q(1, BIG), standard_meter_q(1, BIG + 1))));
}

static void test_sub_overflow_is_refused(void) {
    assert(is_zero(standard_meter_q_sub(standard_meter_q(1, BIG), standard_meter_q(1, BIG + 1))));
}

static void test_mul_int_overflow_is_refused(void) {
    assert(is_zero(standard_meter_q_mul_int(standard_meter_q(LLONG_MAX / 2, 1), 3)));
}

static void test_div_int_overflow_is_refused(void) {
    assert(is_zero(standard_meter_q_div_int(standard_meter_q(1, BIG), BIG + 1)));
    assert(is_zero(standard_meter_q_div_int(standard_meter_q(1, 4), 0)));
}

static void test_snap(void) {
    StandardMeterConfig config = standard_meter_config_default();
    bool changed = true;
    StandardMeterQ on_grid = standard_meter_q(3, 32);
    StandardMeterQ same = standard_meter_snap(&config, on_grid, false, &changed);
    assert(standard_meter_q_compare(same, on_grid) == 0 && !changed);
    /* time * grid does not fit: the time is returned as it is */
    StandardMeterQ huge = standard_meter_q(LLONG_MAX / 2, 1);
    changed = true;
    StandardMeterQ out = standard_meter_snap(&config, huge, false, &changed);
    assert(out.n == huge.n && out.d == huge.d && !changed);
}

int main(void) {
    test_ordinary_values();
    test_add_overflow_is_refused();
    test_sub_overflow_is_refused();
    test_mul_int_overflow_is_refused();
    test_div_int_overflow_is_refused();
    test_snap();
    printf("standard_meter_analysis_time: ok\n");
    return 0;
}

#include "time/performance_time.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

static PerformanceTime *make(long n, unsigned long d) {
    PerformanceTime *t = NULL;
    assert(performance_time_create(n, d, &t) == PERFORMANCE_TIME_OK);
    return t;
}

static void test_reduces_to_lowest_terms(void) {
    PerformanceTime *t = make(6, 4);
    assert(performance_time_numerator(t) == 3);
    assert(performance_time_denominator(t) == 2);
    performance_time_destroy(t);

    t = make(0, 7);
    assert(performance_time_numerator(t) == 0);
    assert(performance_time_denominator(t) == 1);
    performance_time_destroy(t);
}

static void test_rejects_bad_input(void) {
    PerformanceTime *t = NULL;
    assert(performance_time_create(1, 0, &t) == PERFORMANCE_TIME_ERR_NON_POSITIVE_DENOMINATOR);
    assert(performance_time_create(-1, 2, &t) == PERFORMANCE_TIME_ERR_NEGATIVE);
    assert(performance_time_create(1, 2, NULL) == PERFORMANCE_TIME_ERR_NULL_ARGUMENT);
    assert(t == NULL);
}

static void test_equality_is_by_value(void) {
    PerformanceTime *a = make(1, 2);
    PerformanceTime *b = make(2, 4);
    PerformanceTime *c = make(1, 3);
    assert(performance_time_equals(a, b));
    assert(!performance_time_equals(a, c));
    assert(!performance_time_equals(a, NULL));
    performance_time_destroy(a);
    performance_time_destroy(b);
    performance_time_destroy(c);
}

static void test_compare(void) {
    PerformanceTime *half = make(1, 2);
    PerformanceTime *third = make(1, 3);
    PerformanceTime *two = make(2, 1);
    PerformanceTime *five_halves = make(5, 2);
    assert(performance_time_compare(third, half) < 0);
    assert(performance_time_compare(half, third) > 0);
    assert(performance_time_compare(half, half) == 0);
    assert(performance_time_compare(two, five_halves) < 0);
    assert(performance_time_compare(five_halves, two) > 0);
    assert(performance_time_compare(half, two) < 0);
    performance_time_destroy(half);
    performance_time_destroy(third);
    performance_time_destroy(two);
    performance_time_destroy(five_halves);
}

static void test_compare_cannot_overflow(void) {
    /* Neighbouring fractions with huge parts: a cross-product would wrap, the comparison must not. */
    PerformanceTime *a = make(LONG_MAX, ULONG_MAX - 1);
    PerformanceTime *b = make(LONG_MAX - 1, ULONG_MAX - 1);
    assert(performance_time_compare(a, b) > 0);
    assert(performance_time_compare(b, a) < 0);
    assert(performance_time_compare(a, a) == 0);
    performance_time_destroy(a);
    performance_time_destroy(b);
}

static void test_add(void) {
    PerformanceTime *a = make(1, 6);
    PerformanceTime *b = make(1, 3);
    PerformanceTime *sum = NULL;
    assert(performance_time_add(a, b, &sum) == PERFORMANCE_TIME_OK);
    assert(performance_time_numerator(sum) == 1);
    assert(performance_time_denominator(sum) == 2);
    performance_time_destroy(sum);
    performance_time_destroy(a);
    performance_time_destroy(b);
}

static void test_add_overflow_is_an_error_not_a_wrap(void) {
    PerformanceTime *a = make(LONG_MAX, 1);
    PerformanceTime *one = make(1, 1);
    PerformanceTime *sum = NULL;
    assert(performance_time_add(a, one, &sum) == PERFORMANCE_TIME_ERR_OVERFLOW);
    assert(sum == NULL);

    /* Coprime huge denominators: the common denominator does not fit. */
    PerformanceTime *x = make(1, ULONG_MAX);
    PerformanceTime *y = make(1, ULONG_MAX - 1);
    assert(performance_time_add(x, y, &sum) == PERFORMANCE_TIME_ERR_OVERFLOW);
    assert(sum == NULL);

    performance_time_destroy(a);
    performance_time_destroy(one);
    performance_time_destroy(x);
    performance_time_destroy(y);
}

int main(void) {
    test_reduces_to_lowest_terms();
    test_rejects_bad_input();
    test_equality_is_by_value();
    test_compare();
    test_compare_cannot_overflow();
    test_add();
    test_add_overflow_is_an_error_not_a_wrap();
    printf("test_performance_time: ok\n");
    return 0;
}

#include "time/metrical_position.h"

#include <assert.h>
#include <stdio.h>

static Duration *dur(long n, unsigned long d) {
    Duration *x = NULL;
    assert(duration_create(n, d, &x) == DURATION_OK);
    return x;
}

static MetricalPosition *make(int measure, long n, unsigned long d) {
    Duration *offset = dur(n, d);
    MetricalPosition *p = NULL;
    assert(metrical_position_create(measure, offset, &p) == METRICAL_POSITION_OK);
    duration_destroy(offset);
    return p;
}

static void test_holds_measure_and_offset(void) {
    MetricalPosition *p = make(3, 3, 8);
    assert(metrical_position_measure(p) == 3);
    Duration *expected = dur(3, 8);
    assert(duration_equals(metrical_position_offset(p), expected));
    duration_destroy(expected);
    metrical_position_destroy(p);
}

static void test_a_pickup_is_measure_zero(void) {
    MetricalPosition *p = make(0, 3, 4);
    assert(metrical_position_measure(p) == 0);
    metrical_position_destroy(p);
}

static void test_offset_is_copied_and_reduced(void) {
    Duration *offset = dur(6, 16);
    MetricalPosition *p = NULL;
    assert(metrical_position_create(1, offset, &p) == METRICAL_POSITION_OK);
    duration_destroy(offset); /* the position does not borrow it */
    assert(duration_numerator(metrical_position_offset(p)) == 3);
    assert(duration_denominator(metrical_position_offset(p)) == 8);
    metrical_position_destroy(p);
}

static void test_equality_is_by_value(void) {
    MetricalPosition *a = make(2, 1, 2);
    MetricalPosition *b = make(2, 2, 4);
    MetricalPosition *other_measure = make(3, 1, 2);
    MetricalPosition *other_offset = make(2, 1, 4);
    assert(metrical_position_equals(a, b));
    assert(!metrical_position_equals(a, other_measure));
    assert(!metrical_position_equals(a, other_offset));
    assert(!metrical_position_equals(a, NULL));
    metrical_position_destroy(a);
    metrical_position_destroy(b);
    metrical_position_destroy(other_measure);
    metrical_position_destroy(other_offset);
}

static void test_rejects_null(void) {
    MetricalPosition *p = NULL;
    Duration *zero = dur(0, 1);
    assert(metrical_position_create(1, NULL, &p) == METRICAL_POSITION_ERR_NULL_ARGUMENT);
    assert(metrical_position_create(1, zero, NULL) == METRICAL_POSITION_ERR_NULL_ARGUMENT);
    assert(p == NULL);
    duration_destroy(zero);
    metrical_position_destroy(NULL);
}

int main(void) {
    test_holds_measure_and_offset();
    test_a_pickup_is_measure_zero();
    test_offset_is_copied_and_reduced();
    test_equality_is_by_value();
    test_rejects_null();
    printf("test_metrical_position: ok\n");
    return 0;
}

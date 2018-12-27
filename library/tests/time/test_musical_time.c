#include "time/musical_time.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_zero_denominator(void) {
    MusicalTime *t = NULL;
    MusicalTimeResult r = musical_time_create(1, 0, &t);
    assert(r == MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR);
    assert(t == NULL);
}

static void test_create_accepts_valid(void) {
    MusicalTime *t = NULL;
    MusicalTimeResult r = musical_time_create(3, 4, &t);
    assert(r == MUSICAL_TIME_OK);
    assert(t != NULL);
    assert(musical_time_numerator(t) == 3);
    assert(musical_time_denominator(t) == 4);
    musical_time_destroy(t);
}

/* No lower bound: negative positions (e.g. pickup measures) are legal. */
static void test_create_accepts_negative_numerator(void) {
    MusicalTime *t = NULL;
    MusicalTimeResult r = musical_time_create(-1, 4, &t);
    assert(r == MUSICAL_TIME_OK);
    assert(musical_time_numerator(t) == -1);
    assert(musical_time_denominator(t) == 4);
    musical_time_destroy(t);
}

static void test_create_reduces_to_lowest_terms(void) {
    MusicalTime *t = NULL;
    musical_time_create(6, 8, &t);
    assert(musical_time_numerator(t) == 3);
    assert(musical_time_denominator(t) == 4);
    musical_time_destroy(t);
}

static void test_create_reduces_negative_to_lowest_terms(void) {
    MusicalTime *t = NULL;
    musical_time_create(-6, 8, &t);
    assert(musical_time_numerator(t) == -3);
    assert(musical_time_denominator(t) == 4);
    musical_time_destroy(t);
}

/* Property: arithmetic preserves exactness — offsets that reduce to the
   same value are equal regardless of the scale they were expressed at. */
static void test_equality_scale_invariant(void) {
    MusicalTime *base = NULL;
    musical_time_create(-3, 4, &base);

    for (unsigned long k = 1; k <= 20; k++) {
        MusicalTime *scaled = NULL;
        musical_time_create(-3L * (long)k, 4UL * k, &scaled);
        assert(musical_time_equals(base, scaled));
        musical_time_destroy(scaled);
    }

    musical_time_destroy(base);
}

static void test_inequality_different_reduced_value(void) {
    MusicalTime *a = NULL;
    MusicalTime *b = NULL;
    musical_time_create(1, 3, &a);
    musical_time_create(1, 4, &b);

    assert(!musical_time_equals(a, b));

    musical_time_destroy(a);
    musical_time_destroy(b);
}

static void test_add_duration(void) {
    MusicalTime *t = NULL;
    Duration *d = NULL;
    MusicalTime *sum = NULL;
    musical_time_create(1, 4, &t);
    duration_create(1, 4, &d);

    MusicalTimeResult r = musical_time_add(t, d, &sum);
    assert(r == MUSICAL_TIME_OK);
    assert(musical_time_numerator(sum) == 1);
    assert(musical_time_denominator(sum) == 2);

    musical_time_destroy(t);
    duration_destroy(d);
    musical_time_destroy(sum);
}

/* add() must accept a Duration and yield a MusicalTime, moving a position
   by a magnitude without collapsing the two types into each other. */
static void test_add_duration_to_negative_position(void) {
    MusicalTime *t = NULL;
    Duration *d = NULL;
    MusicalTime *sum = NULL;
    musical_time_create(-1, 4, &t);
    duration_create(1, 2, &d);

    musical_time_add(t, d, &sum);
    assert(musical_time_numerator(sum) == 1);
    assert(musical_time_denominator(sum) == 4);

    musical_time_destroy(t);
    duration_destroy(d);
    musical_time_destroy(sum);
}

static void test_subtract_yields_duration(void) {
    MusicalTime *a = NULL;
    MusicalTime *b = NULL;
    Duration *diff = NULL;
    musical_time_create(3, 4, &a);
    musical_time_create(1, 4, &b);

    DurationResult r = musical_time_subtract(a, b, &diff);
    assert(r == DURATION_OK);
    assert(duration_numerator(diff) == 1);
    assert(duration_denominator(diff) == 2);

    musical_time_destroy(a);
    musical_time_destroy(b);
    duration_destroy(diff);
}

static void test_subtract_rejects_negative_result(void) {
    MusicalTime *a = NULL;
    MusicalTime *b = NULL;
    Duration *diff = NULL;
    musical_time_create(1, 4, &a);
    musical_time_create(3, 4, &b);

    DurationResult r = musical_time_subtract(a, b, &diff);
    assert(r == DURATION_ERR_NEGATIVE);
    assert(diff == NULL);

    musical_time_destroy(a);
    musical_time_destroy(b);
}

int main(void) {
    test_create_rejects_zero_denominator();
    test_create_accepts_valid();
    test_create_accepts_negative_numerator();
    test_create_reduces_to_lowest_terms();
    test_create_reduces_negative_to_lowest_terms();
    test_equality_scale_invariant();
    test_inequality_different_reduced_value();
    test_add_duration();
    test_add_duration_to_negative_position();
    test_subtract_yields_duration();
    test_subtract_rejects_negative_result();
    return 0;
}
#include "model/time/musical_time.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>

static MusicalTime mt(long long n, long long d) {
    MusicalTime t;
    assert(musical_time_make(n, d, &t) == MUSICAL_TIME_OK);
    return t;
}

static Duration du(long long n, long long d) {
    Duration x;
    assert(duration_make(n, d, &x) == DURATION_OK);
    return x;
}

static void test_make_rejects_zero_denominator(void) {
    MusicalTime keep = mt(1, 2);
    MusicalTime t = keep;
    MusicalTimeResult r = musical_time_make(1, 0, &t);
    assert(r == MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR);
    assert(musical_time_same(t, keep));
}

static void test_make_accepts_valid(void) {
    MusicalTime t;
    MusicalTimeResult r = musical_time_make(3, 4, &t);
    assert(r == MUSICAL_TIME_OK);
    assert(t.value.num == 3);
    assert(t.value.den == 4);
}

/* No lower bound: negative positions (e.g. pickup measures) are legal. */
static void test_make_accepts_negative_numerator(void) {
    MusicalTime t;
    MusicalTimeResult r = musical_time_make(-1, 4, &t);
    assert(r == MUSICAL_TIME_OK);
    assert(t.value.num == -1);
    assert(t.value.den == 4);
}

static void test_make_reduces_to_lowest_terms(void) {
    MusicalTime t = mt(6, 8);
    assert(t.value.num == 3);
    assert(t.value.den == 4);
}

static void test_make_reduces_negative_to_lowest_terms(void) {
    MusicalTime t = mt(-6, 8);
    assert(t.value.num == -3);
    assert(t.value.den == 4);
}

/* A numerator that does not fit is a refusal, not a wrapped value. */
static void test_make_refuses_what_does_not_fit(void) {
    MusicalTime keep = mt(1, 2);
    MusicalTime t = keep;
    assert(musical_time_make(LLONG_MIN, 1, &t) == MUSICAL_TIME_ERR_OVERFLOW);
    assert(musical_time_same(t, keep));
}

/* Property: arithmetic preserves exactness: offsets that reduce to the
   same value are equal regardless of the scale they were expressed at. */
static void test_equality_scale_invariant(void) {
    MusicalTime base = mt(-3, 4);

    for (long long k = 1; k <= 20; k++) {
        MusicalTime scaled = mt(-3 * k, 4 * k);
        assert(musical_time_same(base, scaled));
        assert(musical_time_cmp(base, scaled) == 0);
    }
}

static void test_inequality_different_reduced_value(void) {
    MusicalTime a = mt(1, 3);
    MusicalTime b = mt(1, 4);

    assert(!musical_time_same(a, b));
    assert(musical_time_cmp(a, b) > 0);
}

static void test_add_duration(void) {
    MusicalTime sum;
    MusicalTimeResult r = musical_time_plus(mt(1, 4), du(1, 4), &sum);
    assert(r == MUSICAL_TIME_OK);
    assert(sum.value.num == 1);
    assert(sum.value.den == 2);
}

/* plus() must accept a Duration and yield a MusicalTime, moving a position
   by a magnitude without collapsing the two types into each other. */
static void test_add_duration_to_negative_position(void) {
    MusicalTime sum;
    assert(musical_time_plus(mt(-1, 4), du(1, 2), &sum) == MUSICAL_TIME_OK);
    assert(sum.value.num == 1);
    assert(sum.value.den == 4);
}

int main(void) {
    test_make_rejects_zero_denominator();
    test_make_accepts_valid();
    test_make_accepts_negative_numerator();
    test_make_reduces_to_lowest_terms();
    test_make_reduces_negative_to_lowest_terms();
    test_make_refuses_what_does_not_fit();
    test_equality_scale_invariant();
    test_inequality_different_reduced_value();
    test_add_duration();
    test_add_duration_to_negative_position();
    return 0;
}

#include <assert.h>
#include <limits.h>
#include <stddef.h>

#include "model/core/rational.h"
#include "model/time/duration.h"
#include "model/time/musical_time.h"

/* The time values hold a Rational: reading one is `.value`, building one is musical_time_of or duration_of. */

static void test_musical_time_round_trip(void) {
    MusicalTime t;
    assert(musical_time_make(3, 4, &t) == MUSICAL_TIME_OK);
    Rational r = t.value;
    assert(r.num == 3 && r.den == 4);
    MusicalTime back = musical_time_of(r);
    assert(musical_time_same(t, back));
}

static void test_negative_musical_time(void) {
    MusicalTime t;
    assert(musical_time_make(-3, 4, &t) == MUSICAL_TIME_OK);
    Rational r = t.value;
    assert(r.num == -3 && r.den == 4);
    MusicalTime back = musical_time_of(r);
    assert(musical_time_same(t, back));
}

static void test_duration_reads(void) {
    Duration d;
    assert(duration_make(5, 8, &d) == DURATION_OK);
    Rational r = d.value;
    assert(r.num == 5 && r.den == 8);
    Duration back;
    assert(duration_of(r, &back) == DURATION_OK);
    assert(duration_same(d, back));
}

static void test_does_not_fit(void) {
    /* A value that does not fit a Rational cannot be built: make refuses it and writes nothing. */
    MusicalTime keep = musical_time_of(rational_ratio(7, 9));
    MusicalTime wide = keep;
    assert(musical_time_make(LLONG_MIN, 1, &wide) == MUSICAL_TIME_ERR_OVERFLOW);
    assert(musical_time_same(wide, keep));

    Duration dkeep = {rational_ratio(7, 9)};
    Duration dwide = dkeep;
    assert(duration_make(LLONG_MIN, 1, &dwide) == DURATION_ERR_OVERFLOW);
    assert(duration_same(dwide, dkeep));

    /* A negative rational is a position but not a magnitude. */
    assert(duration_of(rational_ratio(-1, 8), &dwide) == DURATION_ERR_NEGATIVE);
    assert(duration_same(dwide, dkeep));
}

static void test_musical_time_add_overflow(void) {
    MusicalTime t;
    Duration d;
    assert(musical_time_make(LLONG_MAX, 1, &t) == MUSICAL_TIME_OK);
    assert(duration_make(1, 1, &d) == DURATION_OK);
    MusicalTime keep = musical_time_of(rational_ratio(7, 9));
    MusicalTime sum = keep;
    assert(musical_time_plus(t, d, &sum) == MUSICAL_TIME_ERR_OVERFLOW);
    assert(musical_time_same(sum, keep));
}

static void test_duration_arithmetic_overflow(void) {
    Duration a;
    Duration b;
    assert(duration_make(LLONG_MAX, 1, &a) == DURATION_OK);
    assert(duration_make(1, 1, &b) == DURATION_OK);
    Duration keep = {rational_ratio(7, 9)};
    Duration out = keep;
    assert(duration_plus(a, b, &out) == DURATION_ERR_OVERFLOW);
    assert(duration_same(out, keep));
}

static void test_exact_results_unchanged(void) {
    MusicalTime t;
    Duration d;
    MusicalTime sum;
    assert(musical_time_make(1, 3, &t) == MUSICAL_TIME_OK);
    assert(duration_make(1, 6, &d) == DURATION_OK);
    assert(musical_time_plus(t, d, &sum) == MUSICAL_TIME_OK);
    assert(sum.value.num == 1 && sum.value.den == 2);
}

int main(void) {
    test_musical_time_round_trip();
    test_negative_musical_time();
    test_duration_reads();
    test_does_not_fit();
    test_musical_time_add_overflow();
    test_duration_arithmetic_overflow();
    test_exact_results_unchanged();
    return 0;
}

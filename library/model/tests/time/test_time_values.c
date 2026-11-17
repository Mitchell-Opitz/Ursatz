#include "model/time/time_span.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

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

static void test_musical_time_make(void) {
    MusicalTime t = mt(6, 8);
    assert(t.value.num == 3 && t.value.den == 4);
    t = mt(-6, 8);
    assert(t.value.num == -3 && t.value.den == 4);
    t = mt(0, 5);
    assert(t.value.num == 0 && t.value.den == 1);

    MusicalTime keep = mt(1, 2);
    MusicalTime out = keep;
    assert(musical_time_make(1, 0, &out) == MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR);
    assert(musical_time_make(1, -3, &out) == MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR);
    assert(musical_time_make(LLONG_MIN, 1, &out) == MUSICAL_TIME_ERR_OVERFLOW);
    assert(musical_time_same(out, keep));
}

static void test_literal_equals_made_value(void) {
    MusicalTime literal = {{3, 4}};
    MusicalTime made = mt(6, 8);
    assert(memcmp(&literal, &made, sizeof literal) == 0);
    assert(musical_time_same(literal, made));
    assert(musical_time_same(musical_time_of(rational_ratio(9, 12)), literal));
    Duration dl = {{5, 8}};
    assert(duration_same(dl, du(10, 16)));
    Duration dmade = du(5, 8);
    assert(memcmp(&dl, &dmade, sizeof dl) == 0);
}

static void test_musical_time_cmp_and_same(void) {
    assert(musical_time_cmp(mt(1, 3), mt(1, 2)) < 0);
    assert(musical_time_cmp(mt(1, 2), mt(1, 3)) > 0);
    assert(musical_time_cmp(mt(2, 6), mt(1, 3)) == 0);
    assert(musical_time_cmp(mt(-1, 4), mt(1, 4)) < 0);
    assert(musical_time_same(mt(2, 6), mt(1, 3)));
    assert(!musical_time_same(mt(1, 3), mt(1, 2)));

    /* Cross products that overflow 64 bits. */
    MusicalTime a = mt(LLONG_MAX, LLONG_MAX - 1);
    MusicalTime b = mt(LLONG_MAX - 1, LLONG_MAX - 2);
    assert(musical_time_cmp(a, b) < 0);
    assert(musical_time_cmp(b, a) > 0);
    assert(musical_time_cmp(a, a) == 0);
    MusicalTime c = mt(LLONG_MAX - 2, LLONG_MAX);
    MusicalTime d = mt(LLONG_MAX - 4, LLONG_MAX - 2);
    assert(musical_time_cmp(c, d) > 0);
}

static void test_musical_time_plus(void) {
    MusicalTime out;
    assert(musical_time_plus(mt(1, 4), du(1, 4), &out) == MUSICAL_TIME_OK);
    assert(musical_time_same(out, mt(1, 2)));
    assert(musical_time_plus(mt(-1, 2), du(1, 3), &out) == MUSICAL_TIME_OK);
    assert(musical_time_same(out, mt(-1, 6)));

    MusicalTime keep = mt(7, 9);
    out = keep;
    assert(musical_time_plus(mt(LLONG_MAX, 1), du(1, 1), &out) == MUSICAL_TIME_ERR_OVERFLOW);
    assert(musical_time_same(out, keep));
    /* Denominators whose lowest common multiple overflows. */
    assert(musical_time_plus(mt(1, LLONG_MAX), du(1, LLONG_MAX - 1), &out) == MUSICAL_TIME_ERR_OVERFLOW);
    assert(musical_time_same(out, keep));
}

static void test_duration_values(void) {
    Duration d = du(6, 8);
    assert(d.value.num == 3 && d.value.den == 4);
    Duration keep = du(1, 2);
    Duration out = keep;
    assert(duration_make(-1, 4, &out) == DURATION_ERR_NEGATIVE);
    assert(duration_make(1, 0, &out) == DURATION_ERR_NON_POSITIVE_DENOMINATOR);
    assert(duration_make(LLONG_MIN, 1, &out) == DURATION_ERR_OVERFLOW);
    assert(duration_same(out, keep));

    assert(duration_of(rational_ratio(-1, 8), &out) == DURATION_ERR_NEGATIVE);
    assert(duration_same(out, keep));
    assert(duration_of(rational_ratio(0, 1), &out) == DURATION_OK);
    assert(out.value.num == 0 && out.value.den == 1);
    assert(duration_of(rational_ratio(3, 8), &out) == DURATION_OK);
    assert(duration_same(out, du(3, 8)));
}

static void test_duration_cmp_and_plus(void) {
    assert(duration_cmp(du(1, 3), du(1, 2)) < 0);
    assert(duration_cmp(du(1, 2), du(1, 3)) > 0);
    assert(duration_cmp(du(2, 6), du(1, 3)) == 0);
    assert(duration_cmp(du(LLONG_MAX, LLONG_MAX - 1), du(LLONG_MAX - 1, LLONG_MAX - 2)) < 0);

    Duration out;
    assert(duration_plus(du(1, 3), du(1, 6), &out) == DURATION_OK);
    assert(duration_same(out, du(1, 2)));
    Duration keep = du(1, 5);
    out = keep;
    assert(duration_plus(du(LLONG_MAX, 1), du(1, 1), &out) == DURATION_ERR_OVERFLOW);
    assert(duration_same(out, keep));
}

static bool span_same(TimeSpan a, TimeSpan b) {
    return musical_time_same(a.start, b.start) && musical_time_same(a.end, b.end) &&
           memcmp(&a.start, &b.start, sizeof a.start) == 0 && memcmp(&a.end, &b.end, sizeof a.end) == 0;
}

static void test_time_span_values(void) {
    TimeSpan span;
    assert(time_span_make(mt(1, 4), mt(3, 4), &span) == TIME_SPAN_OK);
    assert(musical_time_same(span.start, mt(1, 4)) && musical_time_same(span.end, mt(3, 4)));
    assert(time_span_holds(span));
    Duration len;
    assert(time_span_length(span, &len) == DURATION_OK);
    assert(duration_same(len, du(1, 2)));

    /* An empty span is made, holds nothing, has length zero. */
    TimeSpan empty;
    assert(time_span_make(mt(1, 2), mt(2, 4), &empty) == TIME_SPAN_OK);
    assert(!time_span_holds(empty));
    assert(time_span_length(empty, &len) == DURATION_OK);
    assert(duration_same(len, du(0, 1)));

    TimeSpan keep = span;
    TimeSpan out = keep;
    assert(time_span_make(mt(3, 4), mt(1, 4), &out) == TIME_SPAN_ERR_END_BEFORE_START);
    assert(span_same(out, keep));

    /* Negative starts (pickups) and the length across zero. */
    assert(time_span_make(mt(-1, 4), mt(1, 4), &span) == TIME_SPAN_OK);
    assert(time_span_length(span, &len) == DURATION_OK && duration_same(len, du(1, 2)));

    /* A length that does not fit. */
    assert(time_span_make(mt(-LLONG_MAX, 1), mt(LLONG_MAX, 1), &span) == TIME_SPAN_OK);
    Duration kept = du(1, 7);
    len = kept;
    assert(time_span_length(span, &len) == DURATION_ERR_OVERFLOW);
    assert(duration_same(len, kept));

    /* A span literal equals the made span. */
    TimeSpan literal = {{{1, 4}}, {{3, 4}}};
    assert(time_span_make(mt(1, 4), mt(3, 4), &span) == TIME_SPAN_OK);
    assert(span_same(literal, span));
}

int main(void) {
    test_musical_time_make();
    test_literal_equals_made_value();
    test_musical_time_cmp_and_same();
    test_musical_time_plus();
    test_duration_values();
    test_duration_cmp_and_plus();
    test_time_span_values();
    return 0;
}

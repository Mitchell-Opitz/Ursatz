#include "time/score_time.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/note_value.h"
#include "time/time_signature.h"
#include "time/time_span.h"

/*
 * Score time: the checked span, measure bounds, beat length and snap that the framework time modules wrap. Equal to the
 * old private copies on ordinary values; an overflow is a refusal that leaves the outputs alone.
 */

static TimeSignature *sig(unsigned top, int bottom_exponent) {
    NoteValue *unit = NULL;
    assert(note_value_create(bottom_exponent, &unit) == NOTE_VALUE_OK);
    TimeSignature *ts = NULL;
    assert(time_signature_create(top, unit, &ts) == TIME_SIGNATURE_OK);
    note_value_destroy(unit);
    return ts;
}

static MusicalTime *at(long n, unsigned long d) {
    MusicalTime *t = NULL;
    assert(musical_time_create(n, d, &t) == MUSICAL_TIME_OK);
    return t;
}

static MeterMap *single(unsigned top, int bottom_exponent) {
    TimeSignature *ts = sig(top, bottom_exponent);
    MusicalTime *start = at(0, 1);
    MeterMapChangePoint point = {ts, start};
    EntityID *id = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(ts);
    musical_time_destroy(start);
    return map;
}

static Rational q(long long n, long long d) {
    Rational r;
    assert(rational_make(n, d, &r) == RATIONAL_OK);
    return r;
}

static void test_span(void) {
    MusicalTime *from = at(1, 4);
    MusicalTime *to = at(3, 4);
    TimeSpan *span = NULL;
    assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    Rational start = q(9, 9);
    Rational end = q(9, 9);
    assert(score_time_span(span, &start, &end));
    assert(rational_equals(start, q(1, 4)) && rational_equals(end, q(3, 4)));
    assert(score_time_span(span, NULL, NULL));
    assert(!score_time_span(NULL, &start, &end));
    time_span_destroy(span);

    /* A denominator above LLONG_MAX does not fit a Rational: refused, outputs untouched. */
    MusicalTime *huge = at(1, ULONG_MAX);
    TimeSpan *wide = NULL;
    assert(time_span_create(from, huge, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &wide) == TIME_SPAN_OK);
    start = q(5, 7);
    end = q(5, 7);
    assert(!score_time_span(wide, &start, &end));
    assert(rational_equals(start, q(5, 7)) && rational_equals(end, q(5, 7)));
    time_span_destroy(wide);
    musical_time_destroy(huge);
    musical_time_destroy(from);
    musical_time_destroy(to);
}

static void test_measure_bounds_and_beat(void) {
    MeterMap *simple = single(3, 2); /* 3/4 */
    Rational start;
    Rational end;
    assert(score_time_measure_bounds(simple, 2, &start, &end));
    assert(rational_equals(start, q(3, 4)) && rational_equals(end, q(3, 2)));
    assert(score_time_measure_bounds(simple, 1, NULL, &end) && rational_equals(end, q(3, 4)));
    Rational beat;
    assert(score_time_beat_length(simple, 1, &beat) && rational_equals(beat, q(1, 4)));
    assert(!score_time_beat_length(simple, 1, NULL));
    assert(!score_time_measure_bounds(simple, 0, &start, &end)); /* no measure 0 in a map without a pickup */
    assert(!score_time_measure_bounds(NULL, 1, &start, &end));
    meter_map_destroy(simple);

    MeterMap *compound = single(6, 3); /* 6/8: a beat is three eighths */
    assert(score_time_beat_length(compound, 1, &beat) && rational_equals(beat, q(3, 8)));
    meter_map_destroy(compound);
}

static void test_snap(void) {
    Rational out;
    bool changed = true;
    /* grid 1/32, tolerance 1/64: 1/32 + 1/128 is within 1/64 of 1/32 */
    assert(score_time_snap(q(5, 128), 32, 64, &out, &changed));
    assert(rational_equals(out, q(1, 32)) && changed);
    /* already on the grid: unchanged, changed false */
    assert(score_time_snap(q(3, 32), 32, 64, &out, &changed));
    assert(rational_equals(out, q(3, 32)) && !changed);
    /* further than the tolerance: left alone */
    assert(score_time_snap(q(1, 64), 32, 128, &out, &changed));
    assert(rational_equals(out, q(1, 64)) && !changed);
    /* a half rounds up, a negative time rounds toward minus infinity first */
    assert(score_time_snap(q(1, 64), 32, 2, &out, NULL));
    assert(rational_equals(out, q(1, 32)));
    assert(score_time_snap(q(-1, 64), 32, 2, &out, NULL));
    assert(rational_equals(out, q(0, 1)));
    /* a zero grid or tolerance is no snapping, not an error */
    assert(score_time_snap(q(5, 128), 0, 64, &out, &changed));
    assert(rational_equals(out, q(5, 128)) && !changed);
    assert(score_time_snap(q(5, 128), 32, 0, &out, &changed));
    assert(rational_equals(out, q(5, 128)) && !changed);
    assert(!score_time_snap(q(5, 128), 32, 64, NULL, &changed));
}

static void test_snap_overflow(void) {
    Rational out = q(7, 3);
    bool changed = true;
    /* time * grid does not fit */
    assert(!score_time_snap(q(LLONG_MAX / 2, 1), 32, 64, &out, &changed));
    assert(rational_equals(out, q(7, 3)) && changed);
    /* time less its nearest grid point does not fit: the denominators multiply past LLONG_MAX */
    assert(!score_time_snap(q(1333333333333333334LL, 4000000000000000001LL), 3, 64, &out, &changed));
    assert(rational_equals(out, q(7, 3)) && changed);
}

int main(void) {
    test_span();
    test_measure_bounds_and_beat();
    test_snap();
    test_snap_overflow();
    printf("score_time: ok\n");
    return 0;
}

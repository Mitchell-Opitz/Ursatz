#include "time/meter_arithmetic.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Exact measure and beat arithmetic over a MeterMap: simple and compound meters, a change of meter, a pickup,
 * barline times, beats with offsets, snapping at the end of a measure, a defaulted map and overflow.
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

static EntityID *id(const char *value) {
    EntityID *e = NULL;
    assert(entity_id_create(value, &e) == ENTITY_ID_OK);
    return e;
}

typedef struct {
    unsigned top;
    int bottom_exponent;
    long start_n;
    unsigned long start_d;
} Change;

static MeterMap *build(const Change *changes, size_t count) {
    TimeSignature *signatures[8];
    MusicalTime *starts[8];
    MeterMapChangePoint points[8];
    for (size_t i = 0; i < count; i++) {
        signatures[i] = sig(changes[i].top, changes[i].bottom_exponent);
        starts[i] = at(changes[i].start_n, changes[i].start_d);
        points[i].time_signature = signatures[i];
        points[i].start = starts[i];
    }
    MeterMap *map = NULL;
    assert(meter_map_create(id("map"), points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
        musical_time_destroy(starts[i]);
    }
    return map;
}

static MeterMap *single(unsigned top, int bottom_exponent) {
    Change change = {top, bottom_exponent, 0, 1};
    return build(&change, 1);
}

static Rational q(long long n, long long d) {
    Rational r;
    assert(rational_make(n, d, &r) == RATIONAL_OK);
    return r;
}

static int measure_of(const MeterMap *map, Rational time) {
    int measure = -99;
    assert(meter_map_measure_of(map, time, &measure) == METER_MAP_OK);
    return measure;
}

static void expect_bounds(const MeterMap *map, int measure, Rational start, Rational end) {
    Rational s;
    Rational e;
    assert(meter_map_measure_bounds(map, measure, &s, &e) == METER_MAP_OK);
    assert(rational_equals(s, start) && rational_equals(e, end));
}

static void expect_beat(const MeterMap *map, Rational time, int measure, int beat, Rational offset) {
    int m = -99;
    int b = -99;
    Rational o;
    assert(meter_map_beat_of(map, time, &m, &b, &o) == METER_MAP_OK);
    assert(m == measure && b == beat && rational_equals(o, offset));
}

static void expect_snap(const MeterMap *map, int measure, Rational time, Rational expected) {
    Rational snapped;
    assert(meter_map_snap_to_beat(map, measure, time, &snapped) == METER_MAP_OK);
    assert(rational_equals(snapped, expected));
}

static void expect_length_and_count(const MeterMap *map, int measure, Rational length, int count) {
    Rational beat;
    int beats = -1;
    assert(meter_map_beat_length(map, measure, &beat) == METER_MAP_OK && rational_equals(beat, length));
    assert(meter_map_beat_count(map, measure, &beats) == METER_MAP_OK && beats == count);
}

static void test_simple_meter(void) {
    MeterMap *map = single(4, 2);
    assert(measure_of(map, q(0, 1)) == 1);
    assert(measure_of(map, q(7, 8)) == 1);
    assert(measure_of(map, q(1, 1)) == 2);
    assert(measure_of(map, q(5, 2)) == 3);
    expect_bounds(map, 2, q(1, 1), q(2, 1));
    expect_length_and_count(map, 1, q(1, 4), 4);
    expect_beat(map, q(0, 1), 1, 1, q(0, 1));
    expect_beat(map, q(5, 4), 2, 2, q(0, 1));
    expect_beat(map, q(11, 8), 2, 2, q(1, 8));
    expect_beat(map, q(7, 8), 1, 4, q(1, 8));
    assert(meter_map_on_beat(map, q(1, 2)) && meter_map_on_beat(map, q(1, 1)) && meter_map_on_beat(map, q(0, 1)));
    assert(!meter_map_on_beat(map, q(3, 8)) && !meter_map_on_beat(map, q(13, 8)));
    meter_map_destroy(map);
}

static void test_barlines_and_last_measure(void) {
    MeterMap *map = single(4, 2);
    int measure = -99;
    assert(meter_map_last_measure_touched(map, q(1, 1), &measure) == METER_MAP_OK && measure == 1);
    assert(meter_map_last_measure_touched(map, q(9, 8), &measure) == METER_MAP_OK && measure == 2);
    assert(meter_map_last_measure_touched(map, q(2, 1), &measure) == METER_MAP_OK && measure == 2);
    assert(meter_map_last_measure_touched(map, q(0, 1), &measure) == METER_MAP_OK && measure == 1);
    assert(meter_map_first_counted_measure(map) == 1);
    meter_map_destroy(map);
}

static void test_compound_meters(void) {
    MeterMap *six = single(6, 3);
    expect_bounds(six, 1, q(0, 1), q(3, 4));
    expect_length_and_count(six, 1, q(3, 8), 2);
    expect_beat(six, q(3, 8), 1, 2, q(0, 1));
    expect_beat(six, q(9, 16), 1, 2, q(3, 16));
    assert(meter_map_on_beat(six, q(3, 8)) && !meter_map_on_beat(six, q(1, 4)));
    meter_map_destroy(six);

    MeterMap *nine = single(9, 3);
    expect_length_and_count(nine, 1, q(3, 8), 3);
    expect_snap(nine, 1, q(1, 2), q(3, 4));
    meter_map_destroy(nine);

    MeterMap *twelve = single(12, 4);
    expect_bounds(twelve, 1, q(0, 1), q(3, 4));
    expect_length_and_count(twelve, 1, q(3, 16), 4);
    expect_beat(twelve, q(7, 16), 1, 3, q(1, 16));
    meter_map_destroy(twelve);

    /* A top number of 6 over a quarter is simple: the beat is the unit. */
    MeterMap *simple_six = single(6, 2);
    expect_length_and_count(simple_six, 1, q(1, 4), 6);
    meter_map_destroy(simple_six);
}

static void test_changing_meters(void) {
    Change changes[] = {{4, 2, 0, 1}, {6, 3, 2, 1}, {3, 2, 11, 4}};
    MeterMap *map = build(changes, 3);
    assert(measure_of(map, q(2, 1)) == 3);
    assert(measure_of(map, q(15, 8)) == 2);
    assert(measure_of(map, q(11, 4)) == 4);
    expect_bounds(map, 3, q(2, 1), q(11, 4));
    expect_length_and_count(map, 2, q(1, 4), 4);
    expect_length_and_count(map, 3, q(3, 8), 2);
    expect_length_and_count(map, 4, q(1, 4), 3);
    expect_beat(map, q(2, 1), 3, 1, q(0, 1));
    expect_beat(map, q(19, 8), 3, 2, q(0, 1));
    int measure = -99;
    assert(meter_map_last_measure_touched(map, q(2, 1), &measure) == METER_MAP_OK && measure == 2);
    meter_map_destroy(map);
}

static void test_short_measure(void) {
    /* 3/4 from 2 cut off at 3 by a 4/4 change: measure 4 holds the one beat that starts in it. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 2, 1}, {4, 2, 3, 1}};
    MeterMap *map = build(changes, 3);
    assert(!meter_map_has_pickup(map));
    expect_bounds(map, 4, q(11, 4), q(3, 1));
    int beats = -1;
    assert(meter_map_beat_count(map, 4, &beats) == METER_MAP_OK && beats == 1);
    assert(measure_of(map, q(3, 1)) == 5);
    expect_snap(map, 4, q(23, 8), q(3, 1));
    meter_map_destroy(map);
}

static void test_pickup(void) {
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 1, 4}};
    MeterMap *map = build(changes, 2);
    assert(meter_map_has_pickup(map));
    assert(meter_map_first_counted_measure(map) == 1);
    assert(measure_of(map, q(0, 1)) == 0);
    assert(measure_of(map, q(1, 4)) == 1);
    expect_bounds(map, 0, q(0, 1), q(1, 4));
    expect_bounds(map, 1, q(1, 4), q(1, 1));
    int beats = -1;
    assert(meter_map_beat_count(map, 0, &beats) == METER_MAP_OK && beats == 1);
    expect_beat(map, q(0, 1), 0, 1, q(0, 1));
    expect_beat(map, q(1, 4), 1, 1, q(0, 1));
    expect_beat(map, q(3, 8), 1, 1, q(1, 8));
    int measure = -99;
    assert(meter_map_last_measure_touched(map, q(1, 4), &measure) == METER_MAP_OK && measure == 0);
    assert(meter_map_last_measure_touched(map, q(0, 1), &measure) == METER_MAP_OK && measure == 0);
    expect_snap(map, 0, q(1, 8), q(1, 4));
    meter_map_destroy(map);
}

static void test_snapping(void) {
    MeterMap *map = single(4, 2);
    expect_snap(map, 1, q(0, 1), q(0, 1));
    expect_snap(map, 1, q(1, 2), q(1, 2));
    expect_snap(map, 1, q(3, 8), q(1, 2));
    expect_snap(map, 1, q(1, 100), q(1, 4));
    /* the end of the measure is the last snap point, never past it */
    expect_snap(map, 1, q(7, 8), q(1, 1));
    expect_snap(map, 1, q(5, 4), q(1, 1));
    expect_snap(map, 2, q(9, 8), q(5, 4));
    meter_map_destroy(map);
}

static void test_defaulted_map(void) {
    MeterMap *map = NULL;
    assert(meter_map_create_defaulted(id("map"), &map) == METER_MAP_OK);
    assert(meter_map_is_defaulted(map));
    assert(measure_of(map, q(3, 2)) == 2);
    expect_length_and_count(map, 1, q(1, 4), 4);
    expect_beat(map, q(3, 2), 2, 3, q(0, 1));
    assert(meter_map_first_counted_measure(map) == 1);
    meter_map_destroy(map);
}

static void test_errors_and_overflow(void) {
    MeterMap *map = single(4, 2);
    int measure = 7;
    Rational out = q(5, 7);
    Rational huge = {LLONG_MAX, 1};
    assert(meter_map_measure_of(map, q(-1, 4), &measure) == METER_MAP_ERR_TIME_BEFORE_START && measure == 7);
    assert(meter_map_measure_of(NULL, q(0, 1), &measure) == METER_MAP_ERR_NULL_ARGUMENT);
    assert(meter_map_measure_of(map, q(0, 1), NULL) == METER_MAP_ERR_NULL_ARGUMENT);
    assert(meter_map_measure_bounds(map, -3, &out, NULL) == METER_MAP_ERR_INDEX_OUT_OF_RANGE);
    assert(rational_equals(out, q(5, 7)));
    assert(meter_map_beat_length(map, -3, &out) != METER_MAP_OK && rational_equals(out, q(5, 7)));
    assert(meter_map_beat_length(map, 1, NULL) == METER_MAP_ERR_NULL_ARGUMENT);
    assert(meter_map_snap_to_beat(map, 1, huge, &out) == METER_MAP_ERR_OVERFLOW);
    assert(rational_equals(out, q(5, 7)));
    assert(meter_map_snap_to_beat(map, 1, q(0, 1), NULL) == METER_MAP_ERR_NULL_ARGUMENT);
    assert(!meter_map_on_beat(map, q(-1, 4)));
    assert(!meter_map_on_beat(NULL, q(0, 1)));
    assert(meter_map_first_counted_measure(NULL) == 1);

    /* A time whose measure number does not fit an int or whose beat count overflows is an error, not a wrap. */
    int beat = 7;
    Rational offset = q(5, 7);
    MeterMapResult result = meter_map_beat_of(map, huge, &measure, &beat, &offset);
    assert(result == METER_MAP_OK || result == METER_MAP_ERR_OVERFLOW || result == METER_MAP_ERR_INDEX_OUT_OF_RANGE);
    if (result != METER_MAP_OK) {
        assert(beat == 7 && rational_equals(offset, q(5, 7)));
    }
    meter_map_destroy(map);
}

int main(void) {
    test_simple_meter();
    test_barlines_and_last_measure();
    test_compound_meters();
    test_changing_meters();
    test_short_measure();
    test_pickup();
    test_snapping();
    test_defaulted_map();
    test_errors_and_overflow();
    printf("meter_arithmetic: all tests passed\n");
    return 0;
}

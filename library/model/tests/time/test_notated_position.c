#include "model/time/meter_arithmetic.h"
#include "meter_test_access.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Notated positions (R-588): meter_map_position_of and meter_map_time_of are inverses over 4/4, 6/8, 3+2 over 8 and a
 * meter change with a pickup. Beats are counted from 1 by beat group; the offset runs in whole notes from the beat's
 * start.
 */

typedef struct {
    unsigned top;
    const unsigned *groups; /* NULL: the default groups of top */
    size_t group_count;
    int bottom_exponent;
    long long start_num;
    long long start_den;
} Change;

static Rational q(long long n, long long d) {
    Rational r;
    assert(rational_make(n, d, &r) == RATIONAL_OK);
    return r;
}

static MusicalTime at(long long n, long long d) {
    MusicalTime t;
    assert(musical_time_make(n, d, &t) == MUSICAL_TIME_OK);
    return t;
}

static MeterMap *build(const Change *changes, size_t count) {
    TimeSignature *signatures[4];
    MeterMapChangePoint points[4];
    assert(count <= 4);
    for (size_t i = 0; i < count; i++) {
        NoteValue *unit = NULL;
        assert(note_value_create(changes[i].bottom_exponent, &unit) == NOTE_VALUE_OK);
        if (changes[i].groups != NULL) {
            assert(time_signature_create_grouped(changes[i].groups, changes[i].group_count, unit, &signatures[i]) ==
                   TIME_SIGNATURE_OK);
        } else {
            assert(time_signature_create(changes[i].top, unit, &signatures[i]) == TIME_SIGNATURE_OK);
        }
        note_value_destroy(unit);
        points[i].time_signature = signatures[i];
        points[i].start = at(changes[i].start_num, changes[i].start_den);
    }
    EntityID *id = NULL;
    assert(entity_id_create("position-map", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
    }
    return map;
}

static void expect_position(const MeterMap *map, MusicalTime t, int measure, int beat, Rational offset) {
    NotatedPosition p = {-1, -1, {0, 1}};
    assert(meter_map_position_of(map, t, &p) == METER_MAP_OK);
    assert(p.measure == measure && p.beat == beat && rational_equals(p.offset, offset));
    /* and back: the inverse gives the same time */
    MusicalTime back;
    assert(meter_map_time_of(map, p, &back) == METER_MAP_OK);
    assert(musical_time_cmp(back, t) == 0);
}

/* Every time on a 1/48 grid from 0 to `end` converts to a position and back to itself. */
static void round_trip_grid(const MeterMap *map, long long end_num, long long end_den) {
    for (long long k = 0; k * end_den < end_num * 48; k++) {
        MusicalTime t = at(k, 48);
        NotatedPosition p;
        assert(meter_map_position_of(map, t, &p) == METER_MAP_OK);
        assert(p.beat >= 1 && !rational_is_negative(p.offset));
        MusicalTime back;
        assert(meter_map_time_of(map, p, &back) == METER_MAP_OK);
        assert(musical_time_cmp(back, t) == 0);
    }
}

static void test_four_four(void) {
    Change changes[] = {{4, NULL, 0, 2, 0, 1}};
    MeterMap *map = build(changes, 1);
    expect_position(map, at(0, 1), 1, 1, q(0, 1));
    expect_position(map, at(1, 4), 1, 2, q(0, 1));
    expect_position(map, at(3, 8), 1, 2, q(1, 8));
    expect_position(map, at(3, 4), 1, 4, q(0, 1));
    expect_position(map, at(1, 1), 2, 1, q(0, 1)); /* a barline starts its measure */
    expect_position(map, at(11, 8), 2, 2, q(1, 8));
    expect_position(map, at(7, 2), 4, 3, q(0, 1));
    round_trip_grid(map, 3, 1);
    meter_map_destroy(map);
}

static void test_six_eight(void) {
    Change changes[] = {{6, NULL, 0, 3, 0, 1}};
    MeterMap *map = build(changes, 1);
    expect_position(map, at(0, 1), 1, 1, q(0, 1));
    expect_position(map, at(1, 8), 1, 1, q(1, 8));
    expect_position(map, at(3, 8), 1, 2, q(0, 1)); /* the second beat is a dotted quarter later */
    expect_position(map, at(1, 2), 1, 2, q(1, 8));
    expect_position(map, at(11, 16), 1, 2, q(5, 16));
    expect_position(map, at(3, 4), 2, 1, q(0, 1));
    round_trip_grid(map, 3, 1);
    meter_map_destroy(map);
}

static void test_additive_three_plus_two(void) {
    const unsigned groups[] = {3, 2};
    Change changes[] = {{5, groups, 2, 3, 0, 1}};
    MeterMap *map = build(changes, 1);
    expect_position(map, at(0, 1), 1, 1, q(0, 1));
    expect_position(map, at(1, 4), 1, 1, q(1, 4));
    expect_position(map, at(3, 8), 1, 2, q(0, 1)); /* the second group starts at three eighths */
    expect_position(map, at(7, 16), 1, 2, q(1, 16));
    expect_position(map, at(1, 2), 1, 2, q(1, 8));
    expect_position(map, at(5, 8), 2, 1, q(0, 1));
    round_trip_grid(map, 5, 2);
    meter_map_destroy(map);
}

static void test_meter_change_with_pickup(void) {
    /* a quarter-note pickup, 3/4 from the first barline, 3+2 over 8 from one whole note, 6/8 from 13/8 */
    const unsigned groups[] = {3, 2};
    Change changes[] = {{4, NULL, 0, 2, 0, 1}, {3, NULL, 0, 2, 1, 4}, {5, groups, 2, 3, 1, 1}, {6, NULL, 0, 3, 13, 8}};
    MeterMap *map = build(changes, 4);
    assert(meter_map_has_pickup(map));
    expect_position(map, at(0, 1), 0, 1, q(0, 1));
    expect_position(map, at(1, 8), 0, 1, q(1, 8)); /* the pickup counts its own beats from its start */
    expect_position(map, at(1, 4), 1, 1, q(0, 1));
    expect_position(map, at(1, 2), 1, 2, q(0, 1));
    expect_position(map, at(5, 8), 1, 2, q(1, 8));
    expect_position(map, at(1, 1), 2, 1, q(0, 1));
    expect_position(map, at(11, 8), 2, 2, q(0, 1)); /* 3+2: the second group */
    expect_position(map, at(13, 8), 3, 1, q(0, 1));
    expect_position(map, at(2, 1), 3, 2, q(0, 1)); /* 6/8: the second dotted quarter */
    expect_position(map, at(17, 8), 3, 2, q(1, 8));
    round_trip_grid(map, 3, 1);

    /* time_of refuses what the measure does not hold */
    MusicalTime out = at(9, 9);
    NotatedPosition below = {-1, 1, q(0, 1)};
    assert(meter_map_time_of(map, below, &out) == METER_MAP_ERR_INDEX_OUT_OF_RANGE);
    NotatedPosition beat_zero = {1, 0, q(0, 1)};
    assert(meter_map_time_of(map, beat_zero, &out) == METER_MAP_ERR_INDEX_OUT_OF_RANGE);
    NotatedPosition beat_past = {1, 4, q(0, 1)}; /* 3/4 has three beats */
    assert(meter_map_time_of(map, beat_past, &out) == METER_MAP_ERR_INDEX_OUT_OF_RANGE);
    NotatedPosition negative = {1, 2, q(-1, 8)};
    assert(meter_map_time_of(map, negative, &out) == METER_MAP_ERR_INDEX_OUT_OF_RANGE);
    NotatedPosition past_end = {1, 3, q(1, 4)}; /* the end of measure 1 is the next barline, which is not in it */
    assert(meter_map_time_of(map, past_end, &out) == METER_MAP_ERR_INDEX_OUT_OF_RANGE);
    NotatedPosition last_instant = {1, 3, q(1, 8)};
    assert(meter_map_time_of(map, last_instant, &out) == METER_MAP_OK && musical_time_cmp(out, at(7, 8)) == 0);
    assert(musical_time_cmp(out, at(7, 8)) == 0);

    NotatedPosition p;
    assert(meter_map_position_of(map, at(-1, 4), &p) == METER_MAP_ERR_TIME_BEFORE_START);
    assert(meter_map_position_of(NULL, at(0, 1), &p) == METER_MAP_ERR_NULL_ARGUMENT);
    assert(meter_map_position_of(map, at(0, 1), NULL) == METER_MAP_ERR_NULL_ARGUMENT);
    assert(meter_map_time_of(NULL, below, &out) == METER_MAP_ERR_NULL_ARGUMENT);
    meter_map_destroy(map);
}

/* The output stays untouched on an error. */
static void test_outputs_untouched_on_error(void) {
    Change changes[] = {{4, NULL, 0, 2, 0, 1}};
    MeterMap *map = build(changes, 1);
    NotatedPosition p = {7, 7, q(5, 7)};
    assert(meter_map_position_of(map, at(-1, 2), &p) != METER_MAP_OK);
    assert(p.measure == 7 && p.beat == 7 && rational_equals(p.offset, q(5, 7)));
    MusicalTime t = at(3, 5);
    NotatedPosition bad = {1, 9, q(0, 1)};
    assert(meter_map_time_of(map, bad, &t) != METER_MAP_OK);
    assert(musical_time_cmp(t, at(3, 5)) == 0);
    meter_map_destroy(map);
}

int main(void) {
    test_four_four();
    test_six_eight();
    test_additive_three_plus_two();
    test_meter_change_with_pickup();
    test_outputs_untouched_on_error();
    printf("test_notated_position: ok\n");
    return 0;
}

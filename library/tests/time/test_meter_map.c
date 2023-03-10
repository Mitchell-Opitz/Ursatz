#include "time/meter_map.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * MeterMap tests: a single region, several changes, a mid-measure change, a pickup from a short
 * leading signature or a partial first measure, the defaulted 4/4 case, and lookups at exact
 * barlines (a barline time belongs to the following measure).
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

static int index_at(const MeterMap *map, long n, unsigned long d) {
    MusicalTime *t = at(n, d);
    int index = -99;
    assert(meter_map_measure_index_at(map, t, &index) == METER_MAP_OK);
    musical_time_destroy(t);
    return index;
}

static size_t count_at(const MeterMap *map, long n, unsigned long d) {
    MusicalTime *t = at(n, d);
    size_t count = 99999;
    assert(meter_map_measure_count(map, t, &count) == METER_MAP_OK);
    musical_time_destroy(t);
    return count;
}

static void assert_span(const MeterMap *map, int index, long sn, unsigned long sd, long en, unsigned long ed) {
    TimeSpan *span = NULL;
    assert(meter_map_measure_span(map, index, &span) == METER_MAP_OK);
    MusicalTime *start = at(sn, sd);
    MusicalTime *end = at(en, ed);
    assert(musical_time_equals(time_span_start(span), start));
    assert(musical_time_equals(time_span_end(span), end));
    assert(time_span_boundary(span) == TIME_SPAN_BOUNDARY_LEFT_CLOSED);
    musical_time_destroy(start);
    musical_time_destroy(end);
    time_span_destroy(span);
}

static void test_single_region(void) {
    Change changes[] = {{4, 2, 0, 1}};
    MeterMap *map = build(changes, 1);

    assert(meter_map_region_count(map) == 1);
    assert(!meter_map_is_defaulted(map));
    assert(!meter_map_has_pickup(map));
    assert(meter_map_first_measure_number(map) == 1);
    const MeterMapRegion *region = meter_map_region(map, 0);
    assert(meter_map_region_end(region) == NULL);
    assert(meter_map_region_first_measure_number(region) == 1);
    assert(time_signature_top_number(meter_map_region_time_signature(region)) == 4);

    assert(index_at(map, 0, 1) == 1);
    assert(index_at(map, 3, 4) == 1);
    assert(index_at(map, 5, 4) == 2);
    assert_span(map, 1, 0, 1, 1, 1);
    assert_span(map, 2, 1, 1, 2, 1);
    assert_span(map, 9, 8, 1, 9, 1);

    assert(count_at(map, 0, 1) == 0);
    assert(count_at(map, 4, 1) == 4);
    assert(count_at(map, 9, 2) == 5);

    meter_map_destroy(map);
}

static void test_barline_belongs_to_following_measure(void) {
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 4, 1}};
    MeterMap *map = build(changes, 2);

    assert(index_at(map, 1, 1) == 2);
    assert(index_at(map, 2, 1) == 3);
    assert(index_at(map, 4, 1) == 5); /* the change point is the barline of measure 5 */
    assert(index_at(map, 19, 4) == 6);
    assert(index_at(map, 7, 2) == 4); /* just inside measure 4 */

    const MeterMapRegion *region = NULL;
    MusicalTime *change = at(4, 1);
    assert(meter_map_region_at(map, change, &region) == METER_MAP_OK);
    assert(time_signature_top_number(meter_map_region_time_signature(region)) == 3);
    musical_time_destroy(change);
    MusicalTime *before = at(15, 4);
    assert(meter_map_region_at(map, before, &region) == METER_MAP_OK);
    assert(time_signature_top_number(meter_map_region_time_signature(region)) == 4);
    musical_time_destroy(before);

    meter_map_destroy(map);
}

static void test_multiple_changes(void) {
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 4, 1}, {6, 3, 25, 4}};
    MeterMap *map = build(changes, 3);

    assert(meter_map_region_count(map) == 3);
    assert(meter_map_region_first_measure_number(meter_map_region(map, 0)) == 1);
    assert(meter_map_region_first_measure_number(meter_map_region(map, 1)) == 5);
    /* 3/4 from 4 to 25/4 is 9/4 of a whole note: three whole measures (5, 6, 7). */
    assert(meter_map_region_first_measure_number(meter_map_region(map, 2)) == 8);

    assert_span(map, 4, 3, 1, 4, 1);
    assert_span(map, 5, 4, 1, 19, 4);
    assert_span(map, 7, 11, 2, 25, 4);
    assert_span(map, 8, 25, 4, 7, 1);
    assert(index_at(map, 25, 4) == 8);
    assert(count_at(map, 7, 1) == 8);
    assert(count_at(map, 29, 4) == 9);

    meter_map_destroy(map);
}

static void test_mid_measure_change_is_a_short_measure(void) {
    /* 4/4 for two and a half whole notes, then 3/4: the change cuts measure 3 short. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 5, 2}};
    MeterMap *map = build(changes, 2);

    assert(!meter_map_has_pickup(map));
    assert(meter_map_region_count(map) == 2);
    assert(meter_map_region_first_measure_number(meter_map_region(map, 1)) == 4);

    assert_span(map, 3, 2, 1, 5, 2); /* short: half a whole note, not a full bar */
    assert_span(map, 4, 5, 2, 13, 4);
    assert(index_at(map, 9, 4) == 3);
    assert(index_at(map, 5, 2) == 4);
    assert(count_at(map, 5, 2) == 3);
    assert(count_at(map, 11, 4) == 4);

    meter_map_destroy(map);
}

static void test_pickup_from_short_leading_signature(void) {
    /* 1/8 before 12/8: the leading eighth note is measure 0. */
    Change changes[] = {{1, 3, 0, 1}, {12, 3, 1, 8}};
    MeterMap *map = build(changes, 2);

    assert(meter_map_has_pickup(map));
    assert(meter_map_first_measure_number(map) == 0);
    assert(meter_map_region_is_pickup(meter_map_region(map, 0)));
    assert(!meter_map_region_is_pickup(meter_map_region(map, 1)));
    assert(meter_map_region_first_measure_number(meter_map_region(map, 0)) == 0);
    assert(meter_map_region_first_measure_number(meter_map_region(map, 1)) == 1);

    assert_span(map, 0, 0, 1, 1, 8);
    assert_span(map, 1, 1, 8, 13, 8);
    assert(index_at(map, 1, 16) == 0);
    assert(index_at(map, 1, 8) == 1);
    assert(count_at(map, 1, 8) == 1);
    assert(count_at(map, 13, 8) == 2);

    TimeSpan *span = NULL;
    assert(meter_map_measure_span(map, -1, &span) == METER_MAP_ERR_INDEX_OUT_OF_RANGE);

    meter_map_destroy(map);

    /* 3/8 before 12/8 is also a pickup; 3/8 is under half of the 12/8 measure. */
    Change three_eighths[] = {{3, 3, 0, 1}, {12, 3, 3, 8}};
    map = build(three_eighths, 2);
    assert(meter_map_has_pickup(map));
    meter_map_destroy(map);
}

static void test_pickup_from_partial_first_measure(void) {
    /* A 4/4 measure ended after a quarter note by a 3/4 event. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 1, 4}};
    MeterMap *map = build(changes, 2);

    assert(meter_map_has_pickup(map));
    assert_span(map, 0, 0, 1, 1, 4);
    assert_span(map, 1, 1, 4, 1, 1);
    assert(index_at(map, 1, 4) == 1);

    meter_map_destroy(map);
}

static void test_full_first_bar_is_not_a_pickup(void) {
    /* A real 3/4 first bar before 4/4: 3/4 is not under half of 4/4. */
    Change changes[] = {{3, 2, 0, 1}, {4, 2, 3, 4}};
    MeterMap *map = build(changes, 2);
    assert(!meter_map_has_pickup(map));
    assert(meter_map_first_measure_number(map) == 1);
    assert(meter_map_region_first_measure_number(meter_map_region(map, 1)) == 2);

    meter_map_destroy(map);
}

static void test_defaulted_four_four(void) {
    MeterMap *map = NULL;
    assert(meter_map_create_defaulted(id("map"), &map) == METER_MAP_OK);

    assert(meter_map_is_defaulted(map));
    assert(meter_map_region_count(map) == 1);
    const MeterMapRegion *region = meter_map_region(map, 0);
    assert(meter_map_region_is_defaulted(region));
    assert(time_signature_top_number(meter_map_region_time_signature(region)) == 4);
    assert(time_signature_bottom_number(meter_map_region_time_signature(region)) == 4);
    assert(meter_map_region_end(region) == NULL);
    assert(index_at(map, 1, 1) == 2);

    meter_map_destroy(map);
}

static void test_first_signature_after_zero_gets_a_defaulted_lead_in(void) {
    Change changes[] = {{3, 2, 1, 1}};
    MeterMap *map = build(changes, 1);

    assert(!meter_map_is_defaulted(map));
    assert(meter_map_region_count(map) == 2);
    assert(meter_map_region_is_defaulted(meter_map_region(map, 0)));
    assert(!meter_map_region_is_defaulted(meter_map_region(map, 1)));
    assert(!meter_map_has_pickup(map));
    assert(index_at(map, 1, 1) == 2);

    meter_map_destroy(map);
}

static void test_redundant_change_points(void) {
    /* The same signature on a barline adds nothing; mid-measure it starts a short measure. */
    Change on_barline[] = {{4, 2, 0, 1}, {4, 2, 2, 1}};
    MeterMap *map = build(on_barline, 2);
    assert(meter_map_region_count(map) == 1);
    meter_map_destroy(map);

    Change mid_measure[] = {{4, 2, 0, 1}, {4, 2, 3, 2}};
    map = build(mid_measure, 2);
    assert(meter_map_region_count(map) == 2);
    assert_span(map, 2, 1, 1, 3, 2);
    assert(meter_map_region_first_measure_number(meter_map_region(map, 1)) == 3);
    meter_map_destroy(map);
}

static void test_region_ids_resolve(void) {
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 4, 1}};
    MeterMap *map = build(changes, 2);

    const MeterMapRegion *second = meter_map_region(map, 1);
    const MeterMapRegion *found = NULL;
    assert(meter_map_region_by_id(map, meter_map_region_id(second), &found) == METER_MAP_OK);
    assert(found == second);
    assert(!entity_id_equals(meter_map_region_id(meter_map_region(map, 0)), meter_map_region_id(second)));

    EntityID *unknown = id("nowhere");
    assert(meter_map_region_by_id(map, unknown, &found) == METER_MAP_ERR_NOT_FOUND);
    entity_id_destroy(unknown);

    const MeterMapRegion *of_measure = NULL;
    assert(meter_map_region_of_measure(map, 5, &of_measure) == METER_MAP_OK);
    assert(of_measure == second);

    meter_map_destroy(map);
}

static void test_rejects_bad_input(void) {
    MeterMap *map = NULL;
    assert(meter_map_create(id("map"), NULL, 0, &map) == METER_MAP_ERR_NO_CHANGE_POINTS);

    TimeSignature *ts = sig(4, 2);
    MusicalTime *negative = at(-1, 4);
    MeterMapChangePoint below = {ts, negative};
    assert(meter_map_create(id("map"), &below, 1, &map) == METER_MAP_ERR_NEGATIVE_START);

    MusicalTime *one = at(1, 1);
    MusicalTime *zero = at(0, 1);
    MeterMapChangePoint descending[2] = {{ts, one}, {ts, zero}};
    assert(meter_map_create(id("map"), descending, 2, &map) == METER_MAP_ERR_NOT_ASCENDING);
    MeterMapChangePoint same[2] = {{ts, one}, {ts, one}};
    assert(meter_map_create(id("map"), same, 2, &map) == METER_MAP_ERR_NOT_ASCENDING);

    assert(map == NULL);
    musical_time_destroy(negative);
    musical_time_destroy(one);
    musical_time_destroy(zero);
    time_signature_destroy(ts);
}

static void test_lookup_before_zero_fails(void) {
    MeterMap *map = NULL;
    assert(meter_map_create_defaulted(id("map"), &map) == METER_MAP_OK);
    MusicalTime *negative = at(-1, 8);
    int index = 0;
    assert(meter_map_measure_index_at(map, negative, &index) == METER_MAP_ERR_TIME_BEFORE_START);
    musical_time_destroy(negative);
    meter_map_destroy(map);
}

int main(void) {
    test_single_region();
    test_barline_belongs_to_following_measure();
    test_multiple_changes();
    test_mid_measure_change_is_a_short_measure();
    test_pickup_from_short_leading_signature();
    test_pickup_from_partial_first_measure();
    test_full_first_bar_is_not_a_pickup();
    test_defaulted_four_four();
    test_first_signature_after_zero_gets_a_defaulted_lead_in();
    test_redundant_change_points();
    test_region_ids_resolve();
    test_rejects_bad_input();
    test_lookup_before_zero_fails();
    printf("test_meter_map: ok\n");
    return 0;
}

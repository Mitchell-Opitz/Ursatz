#include "time/tempo_map.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * TempoMap tests: a single tempo, several changes, seconds at and between change points, a
 * seconds-to-time round trip, a duration across a change, the defaulted case, and the overflow
 * guard. Seconds for a whole-note span d at T microseconds per quarter are d * 4 * T / 1,000,000,
 * so at 500,000 (120 quarter notes per minute) a whole note is 2 seconds.
 */

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
    long start_n;
    unsigned long start_d;
    unsigned long tempo;
} Change;

static TempoMap *build(const Change *changes, size_t count) {
    MusicalTime *starts[8];
    TempoMapChangePoint points[8];
    for (size_t i = 0; i < count; i++) {
        starts[i] = at(changes[i].start_n, changes[i].start_d);
        points[i].start = starts[i];
        points[i].microseconds_per_quarter = changes[i].tempo;
    }
    TempoMap *map = NULL;
    assert(tempo_map_create(id("tempo"), points, count, &map) == TEMPO_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        musical_time_destroy(starts[i]);
    }
    return map;
}

/* 500,000 from 0, 1,000,000 from one whole note, 250,000 from two. */
static TempoMap *three_tempos(void) {
    Change changes[] = {{0, 1, 500000}, {1, 1, 1000000}, {2, 1, 250000}};
    return build(changes, 3);
}

static void assert_seconds(const TempoMap *map, long tn, unsigned long td, long sn, unsigned long sd) {
    MusicalTime *t = at(tn, td);
    PerformanceTime *seconds = NULL;
    assert(tempo_map_seconds_at(map, t, &seconds) == TEMPO_MAP_OK);
    PerformanceTime *expected = NULL;
    assert(performance_time_create(sn, sd, &expected) == PERFORMANCE_TIME_OK);
    assert(performance_time_equals(seconds, expected));
    performance_time_destroy(expected);
    performance_time_destroy(seconds);
    musical_time_destroy(t);
}

static unsigned long tempo_at(const TempoMap *map, long n, unsigned long d) {
    MusicalTime *t = at(n, d);
    unsigned long tempo = 0;
    assert(tempo_map_tempo_at(map, t, &tempo) == TEMPO_MAP_OK);
    musical_time_destroy(t);
    return tempo;
}

static void test_single_tempo(void) {
    Change changes[] = {{0, 1, 500000}};
    TempoMap *map = build(changes, 1);

    assert(!tempo_map_is_defaulted(map));
    assert(tempo_map_region_count(map) == 1);
    assert(tempo_map_region_end(tempo_map_region(map, 0)) == NULL);
    assert(tempo_at(map, 0, 1) == 500000);
    assert(tempo_at(map, 7, 3) == 500000);

    assert_seconds(map, 0, 1, 0, 1);
    assert_seconds(map, 1, 4, 1, 2); /* a quarter note at 120 per minute */
    assert_seconds(map, 1, 1, 2, 1);
    assert_seconds(map, 1, 3, 2, 3);
    assert_seconds(map, 9, 2, 9, 1);

    tempo_map_destroy(map);
}

static void test_several_changes_and_change_points(void) {
    TempoMap *map = three_tempos();
    assert(tempo_map_region_count(map) == 3);

    /* A change point belongs to the new region. */
    assert(tempo_at(map, 7, 8) == 500000);
    assert(tempo_at(map, 1, 1) == 1000000);
    assert(tempo_at(map, 15, 8) == 1000000);
    assert(tempo_at(map, 2, 1) == 250000);
    assert(tempo_at(map, 100, 1) == 250000);

    /* At the change points: 2 s for the first whole note, then 4 s for the second. */
    assert_seconds(map, 1, 1, 2, 1);
    assert_seconds(map, 2, 1, 6, 1);
    /* Between them. */
    assert_seconds(map, 1, 2, 1, 1);
    assert_seconds(map, 3, 2, 4, 1);
    assert_seconds(map, 5, 2, 13, 2); /* 6 + half a whole note at 1 s */
    assert_seconds(map, 3, 1, 7, 1);

    PerformanceTime *start = NULL;
    assert(tempo_map_region_seconds_at_start(tempo_map_region(map, 2), &start) == TEMPO_MAP_OK);
    assert(performance_time_numerator(start) == 6 && performance_time_denominator(start) == 1);
    performance_time_destroy(start);

    tempo_map_destroy(map);
}

static void assert_round_trip_time(const TempoMap *map, long n, unsigned long d) {
    MusicalTime *t = at(n, d);
    PerformanceTime *seconds = NULL;
    assert(tempo_map_seconds_at(map, t, &seconds) == TEMPO_MAP_OK);
    MusicalTime *back = NULL;
    assert(tempo_map_time_at_seconds(map, seconds, &back) == TEMPO_MAP_OK);
    assert(musical_time_equals(back, t));
    musical_time_destroy(back);
    performance_time_destroy(seconds);
    musical_time_destroy(t);
}

static void test_seconds_to_time_round_trip(void) {
    TempoMap *map = three_tempos();
    assert_round_trip_time(map, 0, 1);
    assert_round_trip_time(map, 1, 7);
    assert_round_trip_time(map, 1, 1);
    assert_round_trip_time(map, 13, 8);
    assert_round_trip_time(map, 2, 1);
    assert_round_trip_time(map, 17, 3);
    assert_round_trip_time(map, 1001, 960);

    /* And the other way, from seconds: a change point's seconds give back its time. */
    PerformanceTime *six = NULL;
    assert(performance_time_create(6, 1, &six) == PERFORMANCE_TIME_OK);
    MusicalTime *time = NULL;
    assert(tempo_map_time_at_seconds(map, six, &time) == TEMPO_MAP_OK);
    MusicalTime *two = at(2, 1);
    assert(musical_time_equals(time, two));
    PerformanceTime *again = NULL;
    assert(tempo_map_seconds_at(map, time, &again) == TEMPO_MAP_OK);
    assert(performance_time_equals(again, six));
    performance_time_destroy(again);
    musical_time_destroy(two);
    musical_time_destroy(time);
    performance_time_destroy(six);

    PerformanceTime *third = NULL;
    assert(performance_time_create(1, 3, &third) == PERFORMANCE_TIME_OK);
    assert(tempo_map_time_at_seconds(map, third, &time) == TEMPO_MAP_OK);
    assert(tempo_map_seconds_at(map, time, &again) == TEMPO_MAP_OK);
    assert(performance_time_equals(again, third));
    performance_time_destroy(again);
    musical_time_destroy(time);
    performance_time_destroy(third);

    tempo_map_destroy(map);
}

static void assert_duration(const TempoMap *map, TimeSpanBoundary boundary, long an, unsigned long ad, long bn,
                            unsigned long bd, long sn, unsigned long sd) {
    MusicalTime *start = at(an, ad);
    MusicalTime *end = at(bn, bd);
    TimeSpan *span = NULL;
    assert(time_span_create(start, end, boundary, &span) == TIME_SPAN_OK);
    PerformanceTime *seconds = NULL;
    assert(tempo_map_duration_seconds(map, span, &seconds) == TEMPO_MAP_OK);
    PerformanceTime *expected = NULL;
    assert(performance_time_create(sn, sd, &expected) == PERFORMANCE_TIME_OK);
    assert(performance_time_equals(seconds, expected));
    performance_time_destroy(expected);
    performance_time_destroy(seconds);
    time_span_destroy(span);
    musical_time_destroy(start);
    musical_time_destroy(end);
}

static void test_duration_seconds_across_a_change(void) {
    TempoMap *map = three_tempos();

    /* [1/2, 5/2): 1 s to the first change, 4 s through the second region, 1/2 s into the third. */
    assert_duration(map, TIME_SPAN_BOUNDARY_LEFT_CLOSED, 1, 2, 5, 2, 11, 2);
    /* The boundary type does not change the length. */
    assert_duration(map, TIME_SPAN_BOUNDARY_CLOSED, 1, 2, 5, 2, 11, 2);
    assert_duration(map, TIME_SPAN_BOUNDARY_OPEN, 1, 2, 5, 2, 11, 2);
    /* Within one region, and empty. */
    assert_duration(map, TIME_SPAN_BOUNDARY_CLOSED, 5, 4, 7, 4, 3, 2);
    assert_duration(map, TIME_SPAN_BOUNDARY_CLOSED, 3, 2, 3, 2, 0, 1);

    tempo_map_destroy(map);
}

static void test_defaulted_tempo(void) {
    TempoMap *map = NULL;
    assert(tempo_map_create_defaulted(id("tempo"), &map) == TEMPO_MAP_OK);

    assert(tempo_map_is_defaulted(map));
    assert(tempo_map_region_count(map) == 1);
    assert(tempo_map_region_is_defaulted(tempo_map_region(map, 0)));
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(map, 0)) == TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER);
    assert(tempo_at(map, 5, 1) == 500000);
    assert_seconds(map, 1, 1, 2, 1);

    tempo_map_destroy(map);
}

static void test_first_tempo_after_zero_gets_a_defaulted_lead_in(void) {
    Change changes[] = {{1, 1, 1000000}};
    TempoMap *map = build(changes, 1);

    assert(!tempo_map_is_defaulted(map));
    assert(tempo_map_region_count(map) == 2);
    assert(tempo_map_region_is_defaulted(tempo_map_region(map, 0)));
    assert(!tempo_map_region_is_defaulted(tempo_map_region(map, 1)));
    assert(tempo_at(map, 1, 2) == 500000);
    assert(tempo_at(map, 1, 1) == 1000000);
    assert_seconds(map, 1, 1, 2, 1);
    assert_seconds(map, 2, 1, 6, 1);

    tempo_map_destroy(map);
}

static void test_a_repeated_tempo_is_its_own_region(void) {
    Change changes[] = {{0, 1, 500000}, {1, 1, 500000}};
    TempoMap *map = build(changes, 2);
    assert(tempo_map_region_count(map) == 2);
    assert_seconds(map, 2, 1, 4, 1);
    tempo_map_destroy(map);
}

static void test_region_accessors_and_ids(void) {
    TempoMap *map = three_tempos();
    const TempoMapRegion *second = tempo_map_region(map, 1);
    assert(tempo_map_region_microseconds_per_quarter(second) == 1000000);
    MusicalTime *one = at(1, 1);
    MusicalTime *two = at(2, 1);
    assert(musical_time_equals(tempo_map_region_start(second), one));
    assert(musical_time_equals(tempo_map_region_end(second), two));
    musical_time_destroy(one);
    musical_time_destroy(two);
    assert(tempo_map_region(map, 3) == NULL);

    const TempoMapRegion *found = NULL;
    assert(tempo_map_region_by_id(map, tempo_map_region_id(second), &found) == TEMPO_MAP_OK);
    assert(found == second);
    EntityID *unknown = id("nowhere");
    assert(tempo_map_region_by_id(map, unknown, &found) == TEMPO_MAP_ERR_NOT_FOUND);
    entity_id_destroy(unknown);

    tempo_map_destroy(map);
}

static void test_rejects_bad_input(void) {
    TempoMap *map = NULL;
    assert(tempo_map_create(id("tempo"), NULL, 0, &map) == TEMPO_MAP_ERR_NO_CHANGE_POINTS);

    MusicalTime *zero = at(0, 1);
    MusicalTime *one = at(1, 1);
    MusicalTime *negative = at(-1, 4);

    TempoMapChangePoint zero_tempo = {zero, 0};
    assert(tempo_map_create(id("tempo"), &zero_tempo, 1, &map) == TEMPO_MAP_ERR_ZERO_TEMPO);
    TempoMapChangePoint below = {negative, 500000};
    assert(tempo_map_create(id("tempo"), &below, 1, &map) == TEMPO_MAP_ERR_NEGATIVE_START);
    TempoMapChangePoint descending[2] = {{one, 500000}, {zero, 500000}};
    assert(tempo_map_create(id("tempo"), descending, 2, &map) == TEMPO_MAP_ERR_NOT_ASCENDING);
    TempoMapChangePoint same[2] = {{one, 500000}, {one, 600000}};
    assert(tempo_map_create(id("tempo"), same, 2, &map) == TEMPO_MAP_ERR_NOT_ASCENDING);
    assert(map == NULL);

    musical_time_destroy(zero);
    musical_time_destroy(one);
    musical_time_destroy(negative);
}

static void test_lookups_before_zero_fail(void) {
    TempoMap *map = NULL;
    assert(tempo_map_create_defaulted(id("tempo"), &map) == TEMPO_MAP_OK);
    MusicalTime *negative = at(-1, 8);
    unsigned long tempo = 0;
    PerformanceTime *seconds = NULL;
    assert(tempo_map_tempo_at(map, negative, &tempo) == TEMPO_MAP_ERR_TIME_BEFORE_START);
    assert(tempo_map_seconds_at(map, negative, &seconds) == TEMPO_MAP_ERR_TIME_BEFORE_START);
    assert(seconds == NULL);
    musical_time_destroy(negative);
    tempo_map_destroy(map);
}

static void test_overflow_is_an_error_not_a_wrap(void) {
    /* A huge time at the slowest MIDI tempo: the exact seconds do not fit. */
    Change slow[] = {{0, 1, 16777215}};
    TempoMap *map = build(slow, 1);
    MusicalTime *huge = at(LONG_MAX, 1);
    PerformanceTime *seconds = NULL;
    assert(tempo_map_seconds_at(map, huge, &seconds) == TEMPO_MAP_ERR_OVERFLOW);
    assert(seconds == NULL);
    tempo_map_destroy(map);

    /* The same time at the fastest tempo does fit: it is exact, and not rejected for being large. */
    Change fast[] = {{0, 1, 1}};
    map = build(fast, 1);
    assert(tempo_map_seconds_at(map, huge, &seconds) == TEMPO_MAP_OK);
    assert(performance_time_numerator(seconds) == LONG_MAX);
    assert(performance_time_denominator(seconds) == 250000);
    MusicalTime *back = NULL;
    assert(tempo_map_time_at_seconds(map, seconds, &back) == TEMPO_MAP_OK);
    assert(musical_time_equals(back, huge));
    musical_time_destroy(back);
    performance_time_destroy(seconds);

    /* Inverse: this many seconds is a time with no room in a MusicalTime. */
    PerformanceTime *many = NULL;
    assert(performance_time_create(LONG_MAX, 1, &many) == PERFORMANCE_TIME_OK);
    MusicalTime *time = NULL;
    assert(tempo_map_time_at_seconds(map, many, &time) == TEMPO_MAP_ERR_OVERFLOW);
    assert(time == NULL);
    performance_time_destroy(many);
    tempo_map_destroy(map);

    /* Construction refuses a map whose seconds at a change point do not fit, and keeps the id. */
    MusicalTime *zero = at(0, 1);
    TempoMapChangePoint points[2] = {{zero, 16777215}, {huge, 1}};
    EntityID *kept = id("tempo");
    assert(tempo_map_create(kept, points, 2, &map) == TEMPO_MAP_ERR_OVERFLOW);
    assert(map == NULL);
    entity_id_destroy(kept);
    musical_time_destroy(zero);
    musical_time_destroy(huge);
}

int main(void) {
    test_single_tempo();
    test_several_changes_and_change_points();
    test_seconds_to_time_round_trip();
    test_duration_seconds_across_a_change();
    test_defaulted_tempo();
    test_first_tempo_after_zero_gets_a_defaulted_lead_in();
    test_a_repeated_tempo_is_its_own_region();
    test_region_accessors_and_ids();
    test_rejects_bad_input();
    test_lookups_before_zero_fail();
    test_overflow_is_an_error_not_a_wrap();
    printf("test_tempo_map: ok\n");
    return 0;
}

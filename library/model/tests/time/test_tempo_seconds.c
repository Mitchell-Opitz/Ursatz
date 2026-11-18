#include "model/time/tempo_map.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/*
 * Exact seconds over a TempoMap (R-532): absolute regions, a metric modulation (a quarter becomes a dotted quarter),
 * the round trip tempo_map_time_at(tempo_map_seconds_at(t)) == t, the modulation errors and the refusals.
 */

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

static EntityID *id(void) {
    EntityID *e = NULL;
    assert(entity_id_create("tempo-seconds", &e) == ENTITY_ID_OK);
    return e;
}

static TempoMapChangePoint absolute(long long n, long long d, unsigned long microseconds) {
    TempoMapChangePoint point;
    memset(&point, 0, sizeof(point));
    point.start = at(n, d);
    point.microseconds_per_quarter = microseconds;
    point.kind = TEMPO_MAP_CHANGE_ABSOLUTE;
    return point;
}

static TempoMapChangePoint modulation(long long n, long long d, Rational old_value, Rational new_value) {
    TempoMapChangePoint point;
    memset(&point, 0, sizeof(point));
    point.start = at(n, d);
    point.kind = TEMPO_MAP_CHANGE_MODULATION;
    point.old_value = old_value;
    point.new_value = new_value;
    return point;
}

static void expect_seconds(const TempoMap *map, MusicalTime t, long long num, long long den) {
    Rational seconds = q(-9, 1);
    assert(tempo_map_seconds_at(map, t, &seconds) == TEMPO_MAP_OK);
    assert(rational_equals(seconds, q(num, den)));
    MusicalTime back;
    assert(tempo_map_time_at(map, seconds, &back) == TEMPO_MAP_OK);
    assert(musical_time_cmp(back, t) == 0); /* the round trip */
}

static void test_absolute_regions(void) {
    /* 500000 microseconds per quarter is 2 seconds a whole note; 1000000 is 4; 250000 is 1 */
    TempoMapChangePoint points[] = {absolute(0, 1, 500000), absolute(1, 1, 1000000), absolute(2, 1, 250000)};
    TempoMap *map = NULL;
    assert(tempo_map_create(id(), points, 3, &map) == TEMPO_MAP_OK);
    expect_seconds(map, at(0, 1), 0, 1);
    expect_seconds(map, at(1, 4), 1, 2);
    expect_seconds(map, at(1, 1), 2, 1);
    expect_seconds(map, at(3, 2), 4, 1);
    expect_seconds(map, at(2, 1), 6, 1);
    expect_seconds(map, at(9, 4), 25, 4);
    expect_seconds(map, at(7, 3), 19, 3); /* 6 s and a third of a whole note at 1 s a whole note */
    expect_seconds(map, at(100, 1), 6 + 98, 1);
    tempo_map_destroy(map);
}

static void test_modulation(void) {
    /* 120 a quarter, then at the second whole note a quarter becomes a dotted quarter (the dotted quarter takes the
       time the quarter took), then at the third the dotted quarter becomes a quarter again */
    TempoMapChangePoint points[] = {absolute(0, 1, 500000), modulation(1, 1, q(1, 4), q(3, 8)),
                                    modulation(2, 1, q(3, 8), q(1, 4))};
    TempoMap *map = NULL;
    assert(tempo_map_create(id(), points, 3, &map) == TEMPO_MAP_OK);
    assert(tempo_map_region_count(map) == 3);
    /* 333333 1/3 microseconds per quarter: the region reads the whole microsecond, the seconds stay exact */
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(map, 0)) == 500000UL);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(map, 1)) == 333333UL);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(map, 2)) == 500000UL);
    assert(!tempo_map_region_is_defaulted(tempo_map_region(map, 1)));
    expect_seconds(map, at(1, 1), 2, 1);
    expect_seconds(map, at(5, 4), 7, 3);   /* a quarter at 1/3 s: 2 + 1/3 */
    expect_seconds(map, at(3, 2), 8, 3);   /* 2 + 2/3 */
    expect_seconds(map, at(13, 12), 19, 9); /* 1/12 of a whole note is 1/9 s: 2 + 1/9, not a whole microsecond */
    expect_seconds(map, at(2, 1), 10, 3);  /* 2 + 4/3 */
    expect_seconds(map, at(9, 4), 23, 6);  /* back at 500000: 10/3 + 1/2 */
    expect_seconds(map, at(3, 1), 16, 3);  /* 10/3 + 2 */
    tempo_map_destroy(map);

    /* a modulation mid-map after an absolute change resolves against that change's tempo */
    TempoMapChangePoint chained[] = {absolute(0, 1, 500000), absolute(1, 2, 1000000), modulation(1, 1, q(1, 2), q(1, 4))};
    assert(tempo_map_create(id(), chained, 3, &map) == TEMPO_MAP_OK);
    /* the half note of 2 s a half note becomes the quarter: a quarter lasts 2 s, 2000000 per quarter */
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(map, 2)) == 2000000UL);
    expect_seconds(map, at(1, 2), 1, 1);
    expect_seconds(map, at(1, 1), 3, 1);
    expect_seconds(map, at(5, 4), 5, 1);   /* 3 + a quarter at 2 s: 5 */
    tempo_map_destroy(map);
}

static void test_the_default_tempo_is_two_seconds_a_whole_note(void) {
    TempoMap *map = NULL;
    assert(tempo_map_create_defaulted(id(), &map) == TEMPO_MAP_OK);
    expect_seconds(map, at(3, 8), 3, 4);
    tempo_map_destroy(map);
}

/* The map is refused with BAD_MODULATION, builds nothing and keeps the id for the caller. */
static void expect_bad_modulation(const TempoMapChangePoint *points, size_t count) {
    TempoMap *map = (TempoMap *)0x1;
    EntityID *owner = id();
    assert(tempo_map_create(owner, points, count, &map) == TEMPO_MAP_ERR_BAD_MODULATION);
    assert(map == (TempoMap *)0x1);
    entity_id_destroy(owner);
}

static void test_bad_modulations(void) {
    /* a modulation at 0 has no tempo before it */
    TempoMapChangePoint at_zero[] = {modulation(0, 1, q(1, 4), q(3, 8))};
    expect_bad_modulation(at_zero, 1);
    TempoMapChangePoint at_zero_then[] = {modulation(0, 1, q(1, 4), q(3, 8)), absolute(1, 1, 500000)};
    expect_bad_modulation(at_zero_then, 2);

    /* a length of zero or below, on either side */
    TempoMapChangePoint zero_old[] = {absolute(0, 1, 500000), modulation(1, 1, q(0, 1), q(3, 8))};
    expect_bad_modulation(zero_old, 2);
    TempoMapChangePoint zero_new[] = {absolute(0, 1, 500000), modulation(1, 1, q(1, 4), q(0, 1))};
    expect_bad_modulation(zero_new, 2);
    TempoMapChangePoint negative[] = {absolute(0, 1, 500000), modulation(1, 1, q(-1, 4), q(3, 8))};
    expect_bad_modulation(negative, 2);
}

static void test_refusals(void) {
    TempoMapChangePoint points[] = {absolute(0, 1, 500000)};
    TempoMap *map = NULL;
    assert(tempo_map_create(id(), points, 1, &map) == TEMPO_MAP_OK);
    Rational seconds = q(5, 7);
    assert(tempo_map_seconds_at(map, at(-1, 4), &seconds) == TEMPO_MAP_ERR_NEGATIVE_START);
    assert(rational_equals(seconds, q(5, 7))); /* untouched on an error */
    MusicalTime t = at(3, 5);
    assert(tempo_map_time_at(map, q(-1, 2), &t) == TEMPO_MAP_ERR_NEGATIVE_START);
    assert(musical_time_cmp(t, at(3, 5)) == 0);
    assert(tempo_map_seconds_at(NULL, at(0, 1), &seconds) == TEMPO_MAP_ERR_NULL_ARGUMENT);
    assert(tempo_map_seconds_at(map, at(0, 1), NULL) == TEMPO_MAP_ERR_NULL_ARGUMENT);
    assert(tempo_map_time_at(NULL, q(0, 1), &t) == TEMPO_MAP_ERR_NULL_ARGUMENT);
    assert(tempo_map_time_at(map, q(0, 1), NULL) == TEMPO_MAP_ERR_NULL_ARGUMENT);
    tempo_map_destroy(map);
}

int main(void) {
    test_absolute_regions();
    test_modulation();
    test_the_default_tempo_is_two_seconds_a_whole_note();
    test_bad_modulations();
    test_refusals();
    printf("test_tempo_seconds: ok\n");
    return 0;
}

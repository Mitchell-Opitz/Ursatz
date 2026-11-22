#include "model/time/tempo_map.h"



#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* Tempos are microseconds per quarter note, 60,000,000 / bpm. */
#define TEMPO_120_BPM 500000 /* 120 bpm */
#define TEMPO_240_BPM 250000 /* 240 bpm */
#define TEMPO_60_BPM 1000000 /* 60 bpm */
#define TEMPO_100_BPM 600000 /* 100 bpm */

/*
 * TempoMap tests: a single tempo, several changes, the region a time falls in, the defaulted case,
 * and the overflow guard on construction.
 */

static MusicalTime at(long n, unsigned long d) {
    MusicalTime t;
    assert(musical_time_make(n, (long long)d, &t) == MUSICAL_TIME_OK);
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
    TempoMapChangePoint points[8];
    for (size_t i = 0; i < count; i++) {
        points[i].start = at(changes[i].start_n, changes[i].start_d);
        points[i].microseconds_per_quarter = changes[i].tempo;
        points[i].kind = TEMPO_MAP_CHANGE_ABSOLUTE;
    }
    TempoMap *map = NULL;
    assert(tempo_map_create(id("tempo"), points, count, &map) == TEMPO_MAP_OK);
    return map;
}

/* 500,000 from 0, 1,000,000 from one whole note, 250,000 from two. */
static TempoMap *three_tempos(void) {
    Change changes[] = {{0, 1, TEMPO_120_BPM}, {1, 1, TEMPO_60_BPM}, {2, 1, TEMPO_240_BPM}};
    return build(changes, 3);
}

/* The tempo of the region a time falls in: the last region that starts at or before it. */
static unsigned long tempo_at(const TempoMap *map, long n, unsigned long d) {
    MusicalTime t = at(n, d);
    unsigned long tempo = 0;
    for (size_t i = tempo_map_region_count(map); i > 0; i--) {
        const TempoMapRegion *region = tempo_map_region(map, i - 1);
        MusicalTime start = tempo_map_region_start(region);
        if (musical_time_cmp(start, t) <= 0) {
            tempo = tempo_map_region_microseconds_per_quarter(region);
            break;
        }
    }
    return tempo;
}

static void test_single_tempo(void) {
    Change changes[] = {{0, 1, TEMPO_120_BPM}};
    TempoMap *map = build(changes, 1);

    assert(!tempo_map_is_defaulted(map));
    assert(tempo_map_region_count(map) == 1);
    assert(tempo_at(map, 0, 1) == TEMPO_120_BPM);
    assert(tempo_at(map, 7, 3) == TEMPO_120_BPM);

    tempo_map_destroy(map);
}

static void test_several_changes_and_change_points(void) {
    TempoMap *map = three_tempos();
    assert(tempo_map_region_count(map) == 3);

    /* A change point belongs to the new region. */
    assert(tempo_at(map, 7, 8) == TEMPO_120_BPM);
    assert(tempo_at(map, 1, 1) == TEMPO_60_BPM);
    assert(tempo_at(map, 15, 8) == TEMPO_60_BPM);
    assert(tempo_at(map, 2, 1) == TEMPO_240_BPM);
    assert(tempo_at(map, 100, 1) == TEMPO_240_BPM);
    tempo_map_destroy(map);
}

static void test_defaulted_tempo(void) {
    TempoMap *map = NULL;
    assert(tempo_map_create_defaulted(id("tempo"), &map) == TEMPO_MAP_OK);

    assert(tempo_map_is_defaulted(map));
    assert(tempo_map_region_count(map) == 1);
    assert(tempo_map_region_is_defaulted(tempo_map_region(map, 0)));
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(map, 0)) == TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER);
    assert(tempo_at(map, 5, 1) == TEMPO_120_BPM);

    tempo_map_destroy(map);
}

static void test_first_tempo_after_zero_gets_a_defaulted_lead_in(void) {
    Change changes[] = {{1, 1, TEMPO_60_BPM}};
    TempoMap *map = build(changes, 1);

    assert(!tempo_map_is_defaulted(map));
    assert(tempo_map_region_count(map) == 2);
    assert(tempo_map_region_is_defaulted(tempo_map_region(map, 0)));
    assert(!tempo_map_region_is_defaulted(tempo_map_region(map, 1)));
    assert(tempo_at(map, 1, 2) == TEMPO_120_BPM);
    assert(tempo_at(map, 1, 1) == TEMPO_60_BPM);

    tempo_map_destroy(map);
}

static void test_a_repeated_tempo_is_its_own_region(void) {
    Change changes[] = {{0, 1, TEMPO_120_BPM}, {1, 1, TEMPO_120_BPM}};
    TempoMap *map = build(changes, 2);
    assert(tempo_map_region_count(map) == 2);
    tempo_map_destroy(map);
}

static void test_region_accessors(void) {
    TempoMap *map = three_tempos();
    const TempoMapRegion *second = tempo_map_region(map, 1);
    assert(tempo_map_region_microseconds_per_quarter(second) == TEMPO_60_BPM);
    assert(musical_time_same(tempo_map_region_start(second), at(1, 1)));
    assert(tempo_map_region(map, 3) == NULL);
    tempo_map_destroy(map);
}

static void test_rejects_bad_input(void) {
    TempoMap *map = NULL;
    assert(tempo_map_create(id("tempo"), NULL, 0, &map) == TEMPO_MAP_ERR_NO_CHANGE_POINTS);

    MusicalTime zero = at(0, 1);
    MusicalTime one = at(1, 1);
    MusicalTime negative = at(-1, 4);

    TempoMapChangePoint zero_tempo = {zero, 0};
    assert(tempo_map_create(id("tempo"), &zero_tempo, 1, &map) == TEMPO_MAP_ERR_ZERO_TEMPO);
    TempoMapChangePoint below = {negative, TEMPO_120_BPM};
    assert(tempo_map_create(id("tempo"), &below, 1, &map) == TEMPO_MAP_ERR_NEGATIVE_START);
    TempoMapChangePoint descending[2] = {{one, TEMPO_120_BPM}, {zero, TEMPO_120_BPM}};
    assert(tempo_map_create(id("tempo"), descending, 2, &map) == TEMPO_MAP_ERR_NOT_ASCENDING);
    TempoMapChangePoint same[2] = {{one, TEMPO_120_BPM}, {one, TEMPO_100_BPM}};
    assert(tempo_map_create(id("tempo"), same, 2, &map) == TEMPO_MAP_ERR_NOT_ASCENDING);
    assert(map == NULL);
}

static void test_overflow_is_an_error_not_a_wrap(void) {
    /* Construction refuses a map whose seconds at a change point do not fit, and keeps the id. */
    MusicalTime zero = at(0, 1);
    MusicalTime huge = at(LONG_MAX, 1);
    TempoMapChangePoint points[2] = {{zero, 16777215}, {huge, 1}};
    EntityID *kept = id("tempo");
    TempoMap *map = NULL;
    assert(tempo_map_create(kept, points, 2, &map) == TEMPO_MAP_ERR_OVERFLOW);
    assert(map == NULL);
    entity_id_destroy(kept);
}

int main(void) {
    test_single_tempo();
    test_several_changes_and_change_points();
    test_defaulted_tempo();
    test_first_tempo_after_zero_gets_a_defaulted_lead_in();
    test_a_repeated_tempo_is_its_own_region();
    test_region_accessors();
    test_rejects_bad_input();
    test_overflow_is_an_error_not_a_wrap();
    printf("test_tempo_map: ok\n");
    return 0;
}

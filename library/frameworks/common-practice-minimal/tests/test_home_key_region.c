#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_regions.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/*
 * Home key tests: one key, a key returning after a modulation, a tie, overflow, empty and NULL input.
 */

static TimeSpan *span_of(long start_num, unsigned long start_den, long end_num, unsigned long end_den) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    TimeSpan *span = NULL;
    assert(musical_time_create(start_num, start_den, &start) == MUSICAL_TIME_OK);
    assert(musical_time_create(end_num, end_den, &end) == MUSICAL_TIME_OK);
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    musical_time_destroy(start); /* the span keeps its own copies */
    musical_time_destroy(end);
    return span;
}

static void free_spans(CommonPracticeKeyRegion *regions, size_t count) {
    for (size_t i = 0; i < count; i++) {
        time_span_destroy(regions[i].span);
    }
}

/* Whole-note lengths in measures of 1: region i covers [start, start + length). */
static void fill(CommonPracticeKeyRegion *regions, const int *keys, const long *lengths, size_t count) {
    long at = 0;
    for (size_t i = 0; i < count; i++) {
        regions[i] = (CommonPracticeKeyRegion){0};
        regions[i].key_index = keys[i];
        regions[i].span = span_of(at, 1, at + lengths[i], 1);
        at += lengths[i];
    }
}

static void test_one_key(void) {
    CommonPracticeKeyRegion regions[1];
    int keys[1] = {7};
    long lengths[1] = {5};
    fill(regions, keys, lengths, 1);
    size_t home = 99;
    Rational duration = {0, 1};
    assert(common_practice_home_key_region(regions, 1, &home, &duration));
    assert(home == 0 && duration.num == 5 && duration.den == 1);
    free_spans(regions, 1);
}

static void test_return_after_modulation(void) {
    /* C major 4, G major 3, C major 2: C major holds 6, home is its first region. */
    CommonPracticeKeyRegion regions[3];
    int keys[3] = {0, 7, 0};
    long lengths[3] = {4, 3, 2};
    fill(regions, keys, lengths, 3);
    size_t home = 99;
    Rational duration = {0, 1};
    assert(common_practice_home_key_region(regions, 3, &home, &duration));
    assert(home == 0 && duration.num == 6 && duration.den == 1);
    free_spans(regions, 3);

    /* The key that comes second holds the most: home is the first region of that key. */
    CommonPracticeKeyRegion later[4];
    int later_keys[4] = {0, 7, 12, 7};
    long later_lengths[4] = {1, 2, 2, 3};
    fill(later, later_keys, later_lengths, 4);
    assert(common_practice_home_key_region(later, 4, &home, &duration));
    assert(home == 1 && duration.num == 5 && duration.den == 1);
    free_spans(later, 4);
}

static void test_mode_is_part_of_the_key(void) {
    /* C major (0) and C minor (12) are different keys. */
    CommonPracticeKeyRegion regions[2];
    int keys[2] = {0, 12};
    long lengths[2] = {2, 3};
    fill(regions, keys, lengths, 2);
    size_t home = 99;
    assert(common_practice_home_key_region(regions, 2, &home, NULL));
    assert(home == 1);
    free_spans(regions, 2);
}

static void test_tie_takes_the_earliest_key(void) {
    CommonPracticeKeyRegion regions[3];
    int keys[3] = {9, 4, 9};
    long lengths[3] = {2, 4, 2};
    fill(regions, keys, lengths, 3);
    size_t home = 99;
    Rational duration = {0, 1};
    assert(common_practice_home_key_region(regions, 3, &home, &duration));
    assert(home == 0 && duration.num == 4 && duration.den == 1);
    free_spans(regions, 3);
}

static void test_fractional_durations(void) {
    CommonPracticeKeyRegion regions[2];
    regions[0] = (CommonPracticeKeyRegion){0};
    regions[0].key_index = 2;
    regions[0].span = span_of(0, 1, 1, 3);
    regions[1] = (CommonPracticeKeyRegion){0};
    regions[1].key_index = 5;
    regions[1].span = span_of(1, 3, 3, 4);
    size_t home = 99;
    Rational duration = {0, 1};
    assert(common_practice_home_key_region(regions, 2, &home, &duration));
    assert(home == 1 && duration.num == 5 && duration.den == 12);
    free_spans(regions, 2);
}

static void test_overflow_and_bad_input(void) {
    CommonPracticeKeyRegion regions[2];
    regions[0] = (CommonPracticeKeyRegion){0};
    regions[0].key_index = 0;
    regions[0].span = span_of(0, 1, 4611686018427387904L, 1);
    regions[1] = (CommonPracticeKeyRegion){0};
    regions[1].key_index = 0;
    regions[1].span = span_of(0, 1, 4611686018427387904L, 1);
    size_t home = 77;
    Rational duration = {7, 8};
    assert(!common_practice_home_key_region(regions, 2, &home, &duration));
    assert(home == 77 && duration.num == 7 && duration.den == 8);
    free_spans(regions, 2);

    CommonPracticeKeyRegion bad = {0};
    bad.key_index = 3;
    assert(!common_practice_home_key_region(&bad, 1, &home, &duration)); /* no span */
    bad.span = span_of(0, 1, 1, 1);
    bad.key_index = 24;
    assert(!common_practice_home_key_region(&bad, 1, &home, &duration)); /* key outside 0..23 */
    free_spans(&bad, 1);

    assert(!common_practice_home_key_region(&bad, 0, &home, &duration)); /* empty */
    assert(!common_practice_home_key_region(NULL, 1, &home, &duration));
    assert(!common_practice_home_key_region(&bad, 1, NULL, &duration));
    assert(home == 77);
}

int main(void) {
    test_one_key();
    test_return_after_modulation();
    test_mode_is_part_of_the_key();
    test_tie_takes_the_earliest_key();
    test_fractional_durations();
    test_overflow_and_bad_input();
    printf("All home key region tests passed.\n");
    return 0;
}

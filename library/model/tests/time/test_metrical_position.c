#include "model/time/metrical_position.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static Duration dur(long long n, long long d) {
    Duration x;
    assert(duration_make(n, d, &x) == DURATION_OK);
    return x;
}

static MetricalPosition *make(int measure, long n, unsigned long d) {
    MetricalPosition *p = NULL;
    assert(metrical_position_create(measure, dur(n, (long long)d), &p) == METRICAL_POSITION_OK);
    return p;
}

static void test_holds_measure_and_offset(void) {
    MetricalPosition *p = make(3, 3, 8);
    assert(metrical_position_measure(p) == 3);
    assert(duration_same(*metrical_position_offset(p), dur(3, 8)));
    metrical_position_destroy(p);
}

static void test_a_pickup_is_measure_zero(void) {
    MetricalPosition *p = make(0, 3, 4);
    assert(metrical_position_measure(p) == 0);
    metrical_position_destroy(p);
}

static void test_offset_is_copied_and_reduced(void) {
    Duration offset = dur(6, 16);
    MetricalPosition *p = NULL;
    assert(metrical_position_create(1, offset, &p) == METRICAL_POSITION_OK); /* the position copies it */
    assert(metrical_position_offset(p)->value.num == 3);
    assert(metrical_position_offset(p)->value.den == 8);
    metrical_position_destroy(p);
}

static void test_rejects_null(void) {
    MetricalPosition *p = NULL;
    assert(metrical_position_create(1, dur(0, 1), NULL) == METRICAL_POSITION_ERR_NULL_ARGUMENT);
    assert(p == NULL);
    metrical_position_destroy(NULL);
}

/* --- derivation from MusicalTime and MeterMap --- */

static TimeSignature *sig(unsigned top, int bottom_exponent) {
    NoteValue *unit = NULL;
    assert(note_value_create(bottom_exponent, &unit) == NOTE_VALUE_OK);
    TimeSignature *ts = NULL;
    assert(time_signature_create(top, unit, &ts) == TIME_SIGNATURE_OK);
    note_value_destroy(unit);
    return ts;
}

static MusicalTime at(long n, unsigned long d) {
    MusicalTime t;
    assert(musical_time_make(n, (long long)d, &t) == MUSICAL_TIME_OK);
    return t;
}

typedef struct {
    unsigned top;
    int bottom_exponent;
    long start_n;
    unsigned long start_d;
} Change;

static MeterMap *build(const Change *changes, size_t count) {
    TimeSignature *signatures[8];
    MeterMapChangePoint points[8];
    for (size_t i = 0; i < count; i++) {
        signatures[i] = sig(changes[i].top, changes[i].bottom_exponent);
        points[i].time_signature = signatures[i];
        points[i].start = at(changes[i].start_n, changes[i].start_d);
    }
    EntityID *id = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
    }
    return map;
}

/* from_time must give (measure, offset). */
static void assert_position(const MeterMap *map, long tn, unsigned long td, int measure, long on,
                            unsigned long od) {
    MusicalTime t = at(tn, td);
    MetricalPosition *p = NULL;
    assert(metrical_position_from_time(map, &t, &p) == METRICAL_POSITION_OK);
    assert(metrical_position_measure(p) == measure);
    assert(duration_same(*metrical_position_offset(p), dur(on, (long long)od)));
    metrical_position_destroy(p);
}

static void test_four_four(void) {
    Change changes[] = {{4, 2, 0, 1}};
    MeterMap *map = build(changes, 1);
    assert_position(map, 0, 1, 1, 0, 1);
    assert_position(map, 1, 4, 1, 1, 4);
    assert_position(map, 3, 4, 1, 3, 4);
    assert_position(map, 7, 8, 1, 7, 8);
    assert_position(map, 1, 1, 2, 0, 1); /* a barline is the next measure at offset 0 */
    assert_position(map, 9, 4, 3, 1, 4);
    assert_position(map, 7, 1, 8, 0, 1);
    meter_map_destroy(map);
}

static void test_three_four(void) {
    Change changes[] = {{3, 2, 0, 1}};
    MeterMap *map = build(changes, 1);
    assert_position(map, 1, 2, 1, 1, 2);
    assert_position(map, 3, 4, 2, 0, 1);
    assert_position(map, 1, 1, 2, 1, 4);
    assert_position(map, 3, 2, 3, 0, 1);
    meter_map_destroy(map);
}

static void test_six_eight(void) {
    Change changes[] = {{6, 3, 0, 1}}; /* a measure is 3/4 */
    MeterMap *map = build(changes, 1);
    assert_position(map, 1, 8, 1, 1, 8);
    assert_position(map, 1, 2, 1, 1, 2);
    assert_position(map, 3, 4, 2, 0, 1);
    assert_position(map, 7, 8, 2, 1, 8);
    meter_map_destroy(map);
}

static void test_twelve_eight(void) {
    Change changes[] = {{12, 3, 0, 1}}; /* a measure is 3/2 */
    MeterMap *map = build(changes, 1);
    assert_position(map, 3, 8, 1, 3, 8);
    assert_position(map, 1, 1, 1, 1, 1);
    assert_position(map, 3, 2, 2, 0, 1);
    assert_position(map, 7, 4, 2, 1, 4);
    assert_position(map, 3, 1, 3, 0, 1);
    meter_map_destroy(map);
}

static void test_across_a_meter_change(void) {
    /* 4/4 for four measures, then 3/4 (measure 5 starts at four whole notes). */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 4, 1}};
    MeterMap *map = build(changes, 2);
    assert_position(map, 15, 4, 4, 3, 4);
    assert_position(map, 4, 1, 5, 0, 1); /* the change point is a barline */
    assert_position(map, 9, 2, 5, 1, 2);
    assert_position(map, 19, 4, 6, 0, 1);
    assert_position(map, 5, 1, 6, 1, 4);
    meter_map_destroy(map);
}

static void test_short_measure_keeps_its_own_start(void) {
    /* 4/4 until 5/2, then 3/4: measure 3 starts at 2 and is cut short at 5/2. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 5, 2}};
    MeterMap *map = build(changes, 2);
    assert_position(map, 2, 1, 3, 0, 1);
    assert_position(map, 9, 4, 3, 1, 4);
    assert_position(map, 5, 2, 4, 0, 1); /* the short measure ends; the 3/4 measure starts here */
    assert_position(map, 11, 4, 4, 1, 4);
    assert_position(map, 13, 4, 5, 0, 1);
    meter_map_destroy(map);
}

static void test_pickup_in_four_four(void) {
    /* A quarter-note pickup: 4/4 ended after a quarter by 3/4. It sits at offset 3/4 of a 4/4 measure. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 1, 4}};
    MeterMap *map = build(changes, 2);
    assert_position(map, 0, 1, 0, 3, 4);
    assert_position(map, 1, 8, 0, 7, 8);
    assert_position(map, 1, 4, 1, 0, 1);
    assert_position(map, 1, 2, 1, 1, 4);

    const TimeSignature *reading = NULL;
    assert(metrical_position_reading_signature(map, 0, &reading) == METRICAL_POSITION_OK);
    assert(time_signature_top_number(reading) == 4 && time_signature_bottom_number(reading) == 4);
    assert(metrical_position_reading_signature(map, 1, &reading) == METRICAL_POSITION_OK);
    assert(time_signature_top_number(reading) == 3 && time_signature_bottom_number(reading) == 4);
    meter_map_destroy(map);
}

static void test_pickup_in_six_eight(void) {
    /* An eighth-note pickup in 6/8: 6/8 ended after an eighth by 3/4. 5/8 into a 6/8 measure. */
    Change changes[] = {{6, 3, 0, 1}, {3, 2, 1, 8}};
    MeterMap *map = build(changes, 2);
    assert_position(map, 0, 1, 0, 5, 8);
    assert_position(map, 1, 16, 0, 11, 16);
    assert_position(map, 1, 8, 1, 0, 1);

    const TimeSignature *reading = NULL;
    assert(metrical_position_reading_signature(map, 0, &reading) == METRICAL_POSITION_OK);
    assert(time_signature_top_number(reading) == 6 && time_signature_bottom_number(reading) == 8);
    meter_map_destroy(map);
}

static void test_short_leading_signature_pickup_reads_in_the_next_signature(void) {
    /* A 1/8 region before 12/8: the pickup eighth is the last eighth of a 12/8 measure, 11/8. */
    Change changes[] = {{1, 3, 0, 1}, {12, 3, 1, 8}};
    MeterMap *map = build(changes, 2);
    assert_position(map, 0, 1, 0, 11, 8);
    assert_position(map, 1, 8, 1, 0, 1);
    assert_position(map, 1, 2, 1, 3, 8);

    const TimeSignature *reading = NULL;
    assert(metrical_position_reading_signature(map, 0, &reading) == METRICAL_POSITION_OK);
    assert(time_signature_top_number(reading) == 12 && time_signature_bottom_number(reading) == 8);
    assert(metrical_position_reading_signature(map, 1, &reading) == METRICAL_POSITION_OK);
    assert(time_signature_top_number(reading) == 12);
    meter_map_destroy(map);
}

static void test_defaulted_first_signature_after_zero(void) {
    /* A signature declared only at one whole note: a defaulted 4/4 measure comes first. */
    Change changes[] = {{3, 2, 1, 1}};
    MeterMap *map = build(changes, 1);
    assert_position(map, 1, 2, 1, 1, 2);
    assert_position(map, 1, 1, 2, 0, 1);
    assert_position(map, 5, 4, 2, 1, 4);
    meter_map_destroy(map);
}

static void test_errors(void) {
    Change changes[] = {{4, 2, 0, 1}};
    MeterMap *map = build(changes, 1);

    MusicalTime negative = at(-1, 8);
    MetricalPosition *p = NULL;
    assert(metrical_position_from_time(map, &negative, &p) == METRICAL_POSITION_ERR_TIME_BEFORE_START);
    assert(p == NULL);

    assert(metrical_position_from_time(NULL, NULL, &p) == METRICAL_POSITION_ERR_NULL_ARGUMENT);
    const TimeSignature *reading = NULL;
    assert(metrical_position_reading_signature(map, 0, &reading) == METRICAL_POSITION_ERR_MEASURE_OUT_OF_RANGE);
    meter_map_destroy(map);
}

static void test_a_measure_number_past_int_is_an_overflow_error(void) {
    Change changes[] = {{4, 2, 0, 1}};
    MeterMap *map = build(changes, 1);
    /* 3,000,000,000 whole notes in 4/4 is measure 3,000,000,001: past int. */
    MusicalTime far = at(3000000000L, 1);
    MetricalPosition *p = NULL;
    assert(metrical_position_from_time(map, &far, &p) == METRICAL_POSITION_ERR_OVERFLOW);
    assert(p == NULL);
    meter_map_destroy(map);
}

int main(void) {
    test_holds_measure_and_offset();
    test_a_pickup_is_measure_zero();
    test_offset_is_copied_and_reduced();
    test_rejects_null();
    test_four_four();
    test_three_four();
    test_six_eight();
    test_twelve_eight();
    test_across_a_meter_change();
    test_short_measure_keeps_its_own_start();
    test_pickup_in_four_four();
    test_pickup_in_six_eight();
    test_short_leading_signature_pickup_reads_in_the_next_signature();
    test_defaulted_first_signature_after_zero();
    test_errors();
    test_a_measure_number_past_int_is_an_overflow_error();
    printf("test_metrical_position: ok\n");
    return 0;
}

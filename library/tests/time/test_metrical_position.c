#include "time/metrical_position.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static Duration *dur(long n, unsigned long d) {
    Duration *x = NULL;
    assert(duration_create(n, d, &x) == DURATION_OK);
    return x;
}

static MetricalPosition *make(int measure, long n, unsigned long d) {
    Duration *offset = dur(n, d);
    MetricalPosition *p = NULL;
    assert(metrical_position_create(measure, offset, &p) == METRICAL_POSITION_OK);
    duration_destroy(offset);
    return p;
}

static void test_holds_measure_and_offset(void) {
    MetricalPosition *p = make(3, 3, 8);
    assert(metrical_position_measure(p) == 3);
    Duration *expected = dur(3, 8);
    assert(duration_equals(metrical_position_offset(p), expected));
    duration_destroy(expected);
    metrical_position_destroy(p);
}

static void test_a_pickup_is_measure_zero(void) {
    MetricalPosition *p = make(0, 3, 4);
    assert(metrical_position_measure(p) == 0);
    metrical_position_destroy(p);
}

static void test_offset_is_copied_and_reduced(void) {
    Duration *offset = dur(6, 16);
    MetricalPosition *p = NULL;
    assert(metrical_position_create(1, offset, &p) == METRICAL_POSITION_OK);
    duration_destroy(offset); /* the position does not borrow it */
    assert(duration_numerator(metrical_position_offset(p)) == 3);
    assert(duration_denominator(metrical_position_offset(p)) == 8);
    metrical_position_destroy(p);
}

static void test_equality_is_by_value(void) {
    MetricalPosition *a = make(2, 1, 2);
    MetricalPosition *b = make(2, 2, 4);
    MetricalPosition *other_measure = make(3, 1, 2);
    MetricalPosition *other_offset = make(2, 1, 4);
    assert(metrical_position_equals(a, b));
    assert(!metrical_position_equals(a, other_measure));
    assert(!metrical_position_equals(a, other_offset));
    assert(!metrical_position_equals(a, NULL));
    metrical_position_destroy(a);
    metrical_position_destroy(b);
    metrical_position_destroy(other_measure);
    metrical_position_destroy(other_offset);
}

static void test_rejects_null(void) {
    MetricalPosition *p = NULL;
    Duration *zero = dur(0, 1);
    assert(metrical_position_create(1, NULL, &p) == METRICAL_POSITION_ERR_NULL_ARGUMENT);
    assert(metrical_position_create(1, zero, NULL) == METRICAL_POSITION_ERR_NULL_ARGUMENT);
    assert(p == NULL);
    duration_destroy(zero);
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

static MusicalTime *at(long n, unsigned long d) {
    MusicalTime *t = NULL;
    assert(musical_time_create(n, d, &t) == MUSICAL_TIME_OK);
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
    MusicalTime *starts[8];
    MeterMapChangePoint points[8];
    for (size_t i = 0; i < count; i++) {
        signatures[i] = sig(changes[i].top, changes[i].bottom_exponent);
        starts[i] = at(changes[i].start_n, changes[i].start_d);
        points[i].time_signature = signatures[i];
        points[i].start = starts[i];
    }
    EntityID *id = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
        musical_time_destroy(starts[i]);
    }
    return map;
}

/* from_time must give (measure, offset); and to_time of that must give the time back. */
static void assert_position(const MeterMap *map, long tn, unsigned long td, int measure, long on,
                            unsigned long od) {
    MusicalTime *t = at(tn, td);
    MetricalPosition *p = NULL;
    assert(metrical_position_from_time(map, t, &p) == METRICAL_POSITION_OK);
    MetricalPosition *expected = make(measure, on, od);
    assert(metrical_position_equals(p, expected));

    MusicalTime *back = NULL;
    assert(metrical_position_to_time(map, p, &back) == METRICAL_POSITION_OK);
    assert(musical_time_equals(back, t));

    MetricalPosition *again = NULL;
    MusicalTime *from_expected = NULL;
    assert(metrical_position_to_time(map, expected, &from_expected) == METRICAL_POSITION_OK);
    assert(metrical_position_from_time(map, from_expected, &again) == METRICAL_POSITION_OK);
    assert(metrical_position_equals(again, expected));

    metrical_position_destroy(again);
    musical_time_destroy(from_expected);
    musical_time_destroy(back);
    metrical_position_destroy(expected);
    metrical_position_destroy(p);
    musical_time_destroy(t);
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

    /* An offset past the short measure's real length is not in it, though a 4/4 measure would hold it. */
    MetricalPosition *too_far = make(3, 3, 4);
    MusicalTime *time = NULL;
    assert(metrical_position_to_time(map, too_far, &time) == METRICAL_POSITION_ERR_OFFSET_OUT_OF_RANGE);
    metrical_position_destroy(too_far);
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

    /* An offset before where the pickup starts is not in the pickup. */
    MetricalPosition *before = make(0, 1, 2);
    MusicalTime *time = NULL;
    assert(metrical_position_to_time(map, before, &time) == METRICAL_POSITION_ERR_OFFSET_OUT_OF_RANGE);
    metrical_position_destroy(before);
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

    MusicalTime *negative = at(-1, 8);
    MetricalPosition *p = NULL;
    assert(metrical_position_from_time(map, negative, &p) == METRICAL_POSITION_ERR_TIME_BEFORE_START);
    assert(p == NULL);
    musical_time_destroy(negative);

    /* A measure below the first (there is no pickup, so measure 0 is out of range), and an offset
       at or past the measure's length. */
    MusicalTime *time = NULL;
    MetricalPosition *no_such_measure = make(0, 0, 1);
    assert(metrical_position_to_time(map, no_such_measure, &time) == METRICAL_POSITION_ERR_MEASURE_OUT_OF_RANGE);
    metrical_position_destroy(no_such_measure);
    MetricalPosition *whole = make(1, 1, 1);
    assert(metrical_position_to_time(map, whole, &time) == METRICAL_POSITION_ERR_OFFSET_OUT_OF_RANGE);
    metrical_position_destroy(whole);
    assert(time == NULL);

    assert(metrical_position_from_time(NULL, NULL, &p) == METRICAL_POSITION_ERR_NULL_ARGUMENT);
    const TimeSignature *reading = NULL;
    assert(metrical_position_reading_signature(map, 0, &reading) == METRICAL_POSITION_ERR_MEASURE_OUT_OF_RANGE);
    meter_map_destroy(map);
}

static void test_a_measure_number_past_int_is_an_overflow_error(void) {
    Change changes[] = {{4, 2, 0, 1}};
    MeterMap *map = build(changes, 1);
    /* 3,000,000,000 whole notes in 4/4 is measure 3,000,000,001: past int. */
    MusicalTime *far = at(3000000000L, 1);
    MetricalPosition *p = NULL;
    assert(metrical_position_from_time(map, far, &p) == METRICAL_POSITION_ERR_OVERFLOW);
    assert(p == NULL);
    musical_time_destroy(far);
    meter_map_destroy(map);
}

int main(void) {
    test_holds_measure_and_offset();
    test_a_pickup_is_measure_zero();
    test_offset_is_copied_and_reduced();
    test_equality_is_by_value();
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

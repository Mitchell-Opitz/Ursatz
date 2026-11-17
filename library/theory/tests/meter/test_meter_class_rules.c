#include <assert.h>
#include <stdio.h>

#include "theory/meter/meter_class_rules.h"
#include "theory/tests/meter/meter_test_util.h"

/* 2, 3, 4 are simple; 6, 9, 12 are compound; duple, triple, quadruple. */
static void test_classify_the_six_categories(void) {
    struct {
        unsigned top;
        MeterType type;
        MeterGrouping grouping;
    } expected[] = {
        {2, METER_TYPE_SIMPLE, METER_GROUPING_DUPLE},        {3, METER_TYPE_SIMPLE, METER_GROUPING_TRIPLE},
        {4, METER_TYPE_SIMPLE, METER_GROUPING_QUADRUPLE},    {6, METER_TYPE_COMPOUND, METER_GROUPING_DUPLE},
        {9, METER_TYPE_COMPOUND, METER_GROUPING_TRIPLE},     {12, METER_TYPE_COMPOUND, METER_GROUPING_QUADRUPLE},
    };
    for (int i = 0; i < 6; i++) {
        /* the same top number reads the same over every bottom number */
        for (int exponent = 0; exponent <= 4; exponent++) {
            TimeSignature *time_signature = make_signature(expected[i].top, exponent);
            MeterClass meter = standard_meter_classify(time_signature);
            assert(meter.type == expected[i].type);
            assert(meter.grouping == expected[i].grouping);
            assert(standard_meter_type(time_signature) == expected[i].type);
            time_signature_destroy(time_signature);
        }
    }
}

/* Anything else -- 5/8, 7/8, 3 stays simple, 15, 18 -- is unclassified, not invalid. */
static void test_unclassified_tops(void) {
    unsigned tops[] = {1, 5, 7, 8, 10, 11, 13, 15, 18, 64};
    for (int i = 0; i < 10; i++) {
        TimeSignature *time_signature = make_signature(tops[i], 3);
        MeterClass meter = standard_meter_classify(time_signature);
        assert(meter.type == METER_TYPE_UNCLASSIFIED);
        assert(meter.grouping == METER_GROUPING_NONE);

        unsigned beats = 77;
        Duration *beat = NULL;
        assert(!standard_meter_beats_per_measure(time_signature, &beats));
        assert(!standard_meter_beat_duration(time_signature, &beat));
        time_signature_destroy(time_signature);
    }

    MeterClass null_meter = standard_meter_classify(NULL);
    assert(null_meter.type == METER_TYPE_UNCLASSIFIED && null_meter.grouping == METER_GROUPING_NONE);
}

/* Simple tops count beats; compound tops count divisions, three to a beat. */
static void test_beats_per_measure(void) {
    unsigned tops[] = {2, 3, 4, 6, 9, 12};
    unsigned beats_expected[] = {2, 3, 4, 2, 3, 4};
    for (int i = 0; i < 6; i++) {
        TimeSignature *time_signature = make_signature(tops[i], 3);
        unsigned beats = 0;
        assert(standard_meter_beats_per_measure(time_signature, &beats));
        assert(beats == beats_expected[i]);
        time_signature_destroy(time_signature);
    }
}

/* A simple beat divides into 2 then 4; a compound beat into 3 then 6. */
static void test_beat_division(void) {
    unsigned division = 0;
    unsigned subdivision = 0;
    assert(standard_meter_beat_division(METER_TYPE_SIMPLE, &division, &subdivision));
    assert(division == 2 && subdivision == 4);
    assert(standard_meter_beat_division(METER_TYPE_COMPOUND, &division, &subdivision));
    assert(division == 3 && subdivision == 6);

    division = 99;
    assert(!standard_meter_beat_division(METER_TYPE_UNCLASSIFIED, &division, &subdivision));
    assert(division == 99);
}

/* The category table: grouping and division for each of the six. */
static void test_category_table(void) {
    unsigned beats;
    unsigned division;

    assert(standard_meter_category(METER_TYPE_SIMPLE, METER_GROUPING_DUPLE, &beats, &division));
    assert(beats == 2 && division == 2);
    assert(standard_meter_category(METER_TYPE_SIMPLE, METER_GROUPING_TRIPLE, &beats, &division));
    assert(beats == 3 && division == 2);
    assert(standard_meter_category(METER_TYPE_SIMPLE, METER_GROUPING_QUADRUPLE, &beats, &division));
    assert(beats == 4 && division == 2);
    assert(standard_meter_category(METER_TYPE_COMPOUND, METER_GROUPING_DUPLE, &beats, &division));
    assert(beats == 2 && division == 3);
    assert(standard_meter_category(METER_TYPE_COMPOUND, METER_GROUPING_TRIPLE, &beats, &division));
    assert(beats == 3 && division == 3);
    assert(standard_meter_category(METER_TYPE_COMPOUND, METER_GROUPING_QUADRUPLE, &beats, &division));
    assert(beats == 4 && division == 3);

    beats = 55;
    assert(!standard_meter_category(METER_TYPE_UNCLASSIFIED, METER_GROUPING_DUPLE, &beats, &division));
    assert(!standard_meter_category(METER_TYPE_SIMPLE, METER_GROUPING_NONE, &beats, &division));
    assert(beats == 55);
}

/* Simple beat is the bottom unit; compound beat is three of it, dotted. */
static void test_beat_duration_and_value(void) {
    /* 4/4: quarter, 1/4 */
    TimeSignature *four_four = make_signature(4, 2);
    Duration *beat = NULL;
    assert(standard_meter_beat_duration(four_four, &beat));
    assert_duration(beat, 1, 4);
    duration_destroy(beat);
    time_signature_destroy(four_four);

    /* a bottom number of 2 is a dotted whole; 1 a dotted breve */
    TimeSignature *six_two = make_signature(6, 1);
    time_signature_destroy(six_two);
    TimeSignature *six_one = make_signature(6, 0);
    time_signature_destroy(six_one);
}

/* The measure is the same length whether the top counts beats or divisions. */
static void test_beats_times_beat_is_measure(void) {
    unsigned tops[] = {2, 3, 4, 6, 9, 12};
    for (int i = 0; i < 6; i++) {
        for (int exponent = 0; exponent <= 4; exponent++) {
            TimeSignature *time_signature = make_signature(tops[i], exponent);
            unsigned beats;
            Duration *beat = NULL;
            Duration *measure = NULL;
            assert(standard_meter_beats_per_measure(time_signature, &beats));
            assert(standard_meter_beat_duration(time_signature, &beat));
            assert(time_signature_measure_duration(time_signature, &measure) == DURATION_OK);
            assert(duration_numerator(beat) * (long)beats * (long)duration_denominator(measure) ==
                   duration_numerator(measure) * (long)duration_denominator(beat));
            duration_destroy(beat);
            duration_destroy(measure);
            time_signature_destroy(time_signature);
        }
    }
}

int main(void) {
    test_classify_the_six_categories();
    test_unclassified_tops();
    test_beats_per_measure();
    test_beat_division();
    test_category_table();
    test_beat_duration_and_value();
    test_beats_times_beat_is_measure();
    printf("test_meter_class_rules (standard-meter): ok\n");
    return 0;
}

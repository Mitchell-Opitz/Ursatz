#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "meter_test_util.h"
#include "metric_position_rules.h"
#include "rule_check.h"

/*
 * Metric position tests: beat and subdivision, metric level and BeatStrength in simple (2/4, 3/4,
 * 4/4, 4/8), compound (6/8, 9/8, 12/8) and irregular (5/8, 7/8) meters. A position is a measure
 * number and an offset in whole notes; the measure number does not matter to the reading.
 */

static MetricalPosition *position(long n, unsigned long d) {
    Duration *offset = make_duration(n, d);
    MetricalPosition *p = NULL;
    assert(metrical_position_create(1, offset, &p) == METRICAL_POSITION_OK);
    duration_destroy(offset);
    return p;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_metric_rule, standard_meter_metric_constraint,
                                    STANDARD_METER_METRIC_RULE_COUNT);
}

/* The offset on_n/on_d into the measure reads as `beat` at sub_n/sub_d of the beat, at the given level
   and strength. */
static void check(const TimeSignature *ts, long on, unsigned long od, unsigned beat, long sub_n,
                  unsigned long sub_d, StandardMeterLevel level, BeatStrength strength) {
    MetricalPosition *p = position(on, od);

    StandardMeterBeatPosition beat_position;
    assert(standard_meter_beat_position(ts, p, &beat_position));
    assert(beat_position.beat == beat);
    assert(beat_position.subdivision_numerator == sub_n);
    assert(beat_position.subdivision_denominator == sub_d);

    StandardMeterLevel got_level;
    assert(standard_meter_metric_level(ts, p, &got_level));
    assert(got_level == level);

    BeatStrength got_strength;
    assert(standard_meter_beat_strength(ts, p, &got_strength));
    assert(got_strength == strength);
    assert(got_strength == standard_meter_level_strength(level));

    metrical_position_destroy(p);
}

static void check_past_the_measure(const TimeSignature *ts, long on, unsigned long od) {
    MetricalPosition *p = position(on, od);
    StandardMeterBeatPosition beat_position = {99, 99, 99};
    StandardMeterLevel level = STANDARD_METER_LEVEL_FINER;
    BeatStrength strength = BEAT_STRENGTH_COUNT;
    assert(!standard_meter_beat_position(ts, p, &beat_position));
    assert(!standard_meter_metric_level(ts, p, &level));
    assert(!standard_meter_beat_strength(ts, p, &strength));
    assert(beat_position.beat == 99 && level == STANDARD_METER_LEVEL_FINER && strength == BEAT_STRENGTH_COUNT);
    metrical_position_destroy(p);
}

#define S BEAT_STRENGTH_STRONG
#define M BEAT_STRENGTH_MEDIUM
#define W BEAT_STRENGTH_WEAK
#define DOWN STANDARD_METER_LEVEL_DOWNBEAT
#define SECOND STANDARD_METER_LEVEL_SECONDARY_BEAT
#define BEAT STANDARD_METER_LEVEL_BEAT
#define OFF STANDARD_METER_LEVEL_OFFBEAT
#define FINE STANDARD_METER_LEVEL_FINER

static void test_two_four(void) {
    TimeSignature *ts = make_signature(2, 2);
    check(ts, 0, 1, 1, 0, 1, DOWN, S);
    check(ts, 1, 4, 2, 0, 1, BEAT, W);
    check(ts, 1, 8, 1, 1, 2, OFF, W); /* the "and" of beat 1 */
    check(ts, 3, 8, 2, 1, 2, OFF, W);
    check(ts, 1, 16, 1, 1, 4, FINE, W); /* a sixteenth: the "e" */
    check(ts, 3, 16, 1, 3, 4, FINE, W); /* the "a" */
    check_past_the_measure(ts, 1, 2);
    time_signature_destroy(ts);
}

static void test_three_four(void) {
    TimeSignature *ts = make_signature(3, 2);
    check(ts, 0, 1, 1, 0, 1, DOWN, S);
    check(ts, 1, 4, 2, 0, 1, BEAT, W);
    check(ts, 1, 2, 3, 0, 1, BEAT, W); /* a triple meter has no secondary beat */
    check(ts, 5, 8, 3, 1, 2, OFF, W);
    check(ts, 9, 16, 3, 1, 4, FINE, W);
    check_past_the_measure(ts, 3, 4);
    time_signature_destroy(ts);
}

static void test_four_four(void) {
    TimeSignature *ts = make_signature(4, 2);
    check(ts, 0, 1, 1, 0, 1, DOWN, S);
    check(ts, 1, 4, 2, 0, 1, BEAT, W);
    check(ts, 1, 2, 3, 0, 1, SECOND, M); /* beat 3 of a quadruple meter */
    check(ts, 3, 4, 4, 0, 1, BEAT, W);
    check(ts, 3, 8, 2, 1, 2, OFF, W);    /* the "and" of beat 2 */
    check(ts, 1, 8, 1, 1, 2, OFF, W);
    check(ts, 5, 8, 3, 1, 2, OFF, W);    /* the "and" of beat 3 is weak, not secondary */
    check(ts, 9, 16, 3, 1, 4, FINE, W);
    check(ts, 15, 16, 4, 3, 4, FINE, W); /* sixteenth-level */
    check_past_the_measure(ts, 1, 1);
    time_signature_destroy(ts);
}

static void test_four_eight_is_also_quadruple(void) {
    TimeSignature *ts = make_signature(4, 3); /* a beat is an eighth */
    check(ts, 1, 4, 3, 0, 1, SECOND, M);
    check(ts, 1, 8, 2, 0, 1, BEAT, W);
    check(ts, 1, 16, 1, 1, 2, OFF, W);
    check_past_the_measure(ts, 1, 2);
    time_signature_destroy(ts);
}

static void test_six_eight(void) {
    TimeSignature *ts = make_signature(6, 3); /* two beats of 3/8 */
    check(ts, 0, 1, 1, 0, 1, DOWN, S);
    check(ts, 3, 8, 2, 0, 1, BEAT, W); /* duple: no secondary beat */
    check(ts, 1, 8, 1, 1, 3, OFF, W);  /* "la": the second eighth */
    check(ts, 1, 4, 1, 2, 3, OFF, W);  /* "li": the third */
    check(ts, 1, 2, 2, 1, 3, OFF, W);
    check(ts, 1, 16, 1, 1, 6, FINE, W); /* the "ta" between the beat and "la" */
    check_past_the_measure(ts, 3, 4);
    time_signature_destroy(ts);
}

static void test_nine_eight(void) {
    TimeSignature *ts = make_signature(9, 3); /* three beats of 3/8 */
    check(ts, 0, 1, 1, 0, 1, DOWN, S);
    check(ts, 3, 8, 2, 0, 1, BEAT, W);
    check(ts, 3, 4, 3, 0, 1, BEAT, W); /* triple: beat 3 is not secondary */
    check(ts, 7, 8, 3, 1, 3, OFF, W);
    check_past_the_measure(ts, 9, 8);
    time_signature_destroy(ts);
}

static void test_twelve_eight(void) {
    TimeSignature *ts = make_signature(12, 3); /* four beats of 3/8 */
    check(ts, 0, 1, 1, 0, 1, DOWN, S);
    check(ts, 3, 8, 2, 0, 1, BEAT, W);
    check(ts, 3, 4, 3, 0, 1, SECOND, M); /* beat 3 of a quadruple meter */
    check(ts, 9, 8, 4, 0, 1, BEAT, W);
    check(ts, 7, 8, 3, 1, 3, OFF, W);
    check(ts, 19, 16, 4, 1, 6, FINE, W);
    check_past_the_measure(ts, 3, 2);
    time_signature_destroy(ts);
}

/* 5/8 and 7/8 are unclassified and have no grouping here: every bottom-unit pulse is a beat, the
   downbeat is the strongest, and no beat is secondary. */
static void test_irregular_meters_every_pulse_is_a_beat(void) {
    TimeSignature *five = make_signature(5, 3);
    check(five, 0, 1, 1, 0, 1, DOWN, S);
    check(five, 1, 8, 2, 0, 1, BEAT, W);
    check(five, 3, 8, 4, 0, 1, BEAT, W);
    check(five, 1, 2, 5, 0, 1, BEAT, W);
    check(five, 1, 16, 1, 1, 2, OFF, W);
    check(five, 1, 32, 1, 1, 4, FINE, W);
    check_past_the_measure(five, 5, 8);
    time_signature_destroy(five);

    TimeSignature *seven = make_signature(7, 3);
    check(seven, 0, 1, 1, 0, 1, DOWN, S);
    check(seven, 3, 8, 4, 0, 1, BEAT, W);
    check(seven, 3, 4, 7, 0, 1, BEAT, W);
    check_past_the_measure(seven, 7, 8);
    time_signature_destroy(seven);
}

/* Pickup measures are read at their offset from where a full measure would have started. */
static void test_pickup_offsets(void) {
    TimeSignature *four_four = make_signature(4, 2);
    check(four_four, 3, 4, 4, 0, 1, BEAT, W); /* a quarter-note pickup is beat 4 */
    time_signature_destroy(four_four);

    TimeSignature *twelve_eight = make_signature(12, 3);
    check(twelve_eight, 11, 8, 4, 2, 3, OFF, W); /* a pickup eighth is the last eighth of 12/8 */
    time_signature_destroy(twelve_eight);
}

static void test_level_strength_mapping(void) {
    assert(standard_meter_level_strength(STANDARD_METER_LEVEL_DOWNBEAT) == BEAT_STRENGTH_STRONG);
    assert(standard_meter_level_strength(STANDARD_METER_LEVEL_SECONDARY_BEAT) == BEAT_STRENGTH_MEDIUM);
    assert(standard_meter_level_strength(STANDARD_METER_LEVEL_BEAT) == BEAT_STRENGTH_WEAK);
    assert(standard_meter_level_strength(STANDARD_METER_LEVEL_OFFBEAT) == BEAT_STRENGTH_WEAK);
    assert(standard_meter_level_strength(STANDARD_METER_LEVEL_FINER) == BEAT_STRENGTH_WEAK);
}

static void test_declines_null_and_overflow(void) {
    TimeSignature *ts = make_signature(4, 2);
    MetricalPosition *p = position(1, 4);
    StandardMeterBeatPosition beat_position;
    StandardMeterLevel level;
    BeatStrength strength;
    assert(!standard_meter_beat_position(NULL, p, &beat_position));
    assert(!standard_meter_beat_position(ts, NULL, &beat_position));
    assert(!standard_meter_beat_position(ts, p, NULL));
    assert(!standard_meter_metric_level(NULL, p, &level));
    assert(!standard_meter_beat_strength(ts, NULL, &strength));
    metrical_position_destroy(p);

    /* An offset whose exact arithmetic does not fit is declined, not wrapped into a wrong beat. */
    p = position(LONG_MAX, ULONG_MAX - 1);
    assert(!standard_meter_beat_position(ts, p, &beat_position));
    assert(!standard_meter_beat_strength(ts, p, &strength));
    metrical_position_destroy(p);
    time_signature_destroy(ts);
}

int main(void) {
    test_rules_reference_framework();
    test_two_four();
    test_three_four();
    test_four_four();
    test_four_eight_is_also_quadruple();
    test_six_eight();
    test_nine_eight();
    test_twelve_eight();
    test_irregular_meters_every_pulse_is_a_beat();
    test_pickup_offsets();
    test_level_strength_mapping();
    test_declines_null_and_overflow();
    printf("test_metric_position_rules: ok\n");
    return 0;
}

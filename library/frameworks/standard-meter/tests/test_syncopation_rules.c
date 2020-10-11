#include <assert.h>
#include <stdio.h>

#include "meter_test_util.h"
#include "rule_check.h"
#include "syncopation_rules.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_syncopation_rule, standard_meter_syncopation_constraint,
                                    STANDARD_METER_SYNCOPATION_RULE_COUNT);
}

static bool off_beat(const TimeSignature *time_signature, long numerator, unsigned long denominator) {
    Duration *onset = make_duration(numerator, denominator);
    bool result = standard_meter_is_off_beat(time_signature, onset);
    duration_destroy(onset);
    return result;
}

/* Beats are the starts of the meter class's beats: quarters in 4/4, dotted quarters in 6/8. */
static void test_off_beat(void) {
    TimeSignature *four_four = make_signature(4, 2);
    assert(!off_beat(four_four, 0, 1));
    assert(off_beat(four_four, 1, 8));
    assert(!off_beat(four_four, 1, 4));
    assert(off_beat(four_four, 3, 16));
    assert(!off_beat(four_four, 3, 4));
    assert(!off_beat(four_four, 1, 1)); /* outside the measure */
    time_signature_destroy(four_four);

    TimeSignature *six_eight = make_signature(6, 3);
    assert(!off_beat(six_eight, 3, 8));
    assert(off_beat(six_eight, 1, 8)); /* a division is off the beat */
    assert(off_beat(six_eight, 1, 4));
    time_signature_destroy(six_eight);

    TimeSignature *five_eight = make_signature(5, 3);
    assert(!off_beat(five_eight, 1, 16)); /* unclassified: no beat grid */
    time_signature_destroy(five_eight);
}

static bool sustained(const TimeSignature *time_signature, long onset_n, unsigned long onset_d, long length_n,
                      unsigned long length_d) {
    Duration *onset = make_duration(onset_n, onset_d);
    Duration *length = make_duration(length_n, length_d);
    bool result = standard_meter_syncopated_by_sustain(time_signature, onset, length);
    duration_destroy(onset);
    duration_destroy(length);
    return result;
}

/* Ties and dots: an off-beat note sustained across the next beat line (Example 8, measures 1 and 2). */
static void test_syncopation_by_sustain(void) {
    TimeSignature *four_four = make_signature(4, 2);

    assert(sustained(four_four, 1, 8, 3, 8)); /* an eighth tied to a quarter, starting on the "+" */
    assert(sustained(four_four, 1, 8, 1, 4)); /* a quarter starting on the "+" */
    assert(sustained(four_four, 1, 8, 3, 8)); /* a dotted quarter on the "+" */
    assert(sustained(four_four, 5, 8, 1, 2)); /* off-beat in beat 3, into beat 4 and on */

    assert(!sustained(four_four, 1, 8, 1, 8)); /* an eighth ending exactly on the beat line */
    assert(!sustained(four_four, 0, 1, 1, 2)); /* a half note on the beat is not syncopated */
    assert(!sustained(four_four, 1, 4, 3, 8)); /* starts on beat 2 */
    assert(!sustained(four_four, 1, 1, 1, 8)); /* outside the measure */
    time_signature_destroy(four_four);

    TimeSignature *six_eight = make_signature(6, 3);
    assert(sustained(six_eight, 1, 4, 3, 16)); /* third division, running into beat 2 */
    assert(!sustained(six_eight, 1, 8, 1, 8));
    time_signature_destroy(six_eight);
}

/* A rest on each beat with notes on the divisions (Example 8, measure 3). */
static void test_syncopation_by_rest(void) {
    TimeSignature *four_four = make_signature(4, 2);
    Duration *rest_on_beat_1 = make_duration(0, 1);
    Duration *rest_on_beat_2 = make_duration(1, 4);
    Duration *and_of_one = make_duration(1, 8);
    Duration *and_of_two = make_duration(3, 8);
    Duration *beat_two = make_duration(1, 4);

    assert(standard_meter_syncopated_by_rest(four_four, rest_on_beat_1, and_of_one));
    assert(standard_meter_syncopated_by_rest(four_four, rest_on_beat_2, and_of_two));

    /* the rest is off the beat, or the note is on it: no syncopation from this rest */
    assert(!standard_meter_syncopated_by_rest(four_four, and_of_one, and_of_two));
    assert(!standard_meter_syncopated_by_rest(four_four, rest_on_beat_1, beat_two));

    duration_destroy(rest_on_beat_1);
    duration_destroy(rest_on_beat_2);
    duration_destroy(and_of_one);
    duration_destroy(and_of_two);
    duration_destroy(beat_two);
    time_signature_destroy(four_four);
}

/* An accent on an off-beat note, even with every division filled (Example 8, measure 4). */
static void test_syncopation_by_accent(void) {
    TimeSignature *four_four = make_signature(4, 2);
    Duration *and_of_one = make_duration(1, 8);
    Duration *beat_two = make_duration(1, 4);
    Duration *outside = make_duration(1, 1);

    assert(standard_meter_syncopated_by_accent(four_four, and_of_one, true));
    assert(!standard_meter_syncopated_by_accent(four_four, and_of_one, false));
    assert(!standard_meter_syncopated_by_accent(four_four, beat_two, true));
    assert(!standard_meter_syncopated_by_accent(four_four, outside, true));

    duration_destroy(and_of_one);
    duration_destroy(beat_two);
    duration_destroy(outside);
    time_signature_destroy(four_four);
}

static void test_null_signature(void) {
    Duration *onset = make_duration(1, 8);
    Duration *length = make_duration(1, 4);
    assert(!standard_meter_is_off_beat(NULL, onset));
    assert(!standard_meter_syncopated_by_sustain(NULL, onset, length));
    assert(!standard_meter_syncopated_by_rest(NULL, onset, onset));
    assert(!standard_meter_syncopated_by_accent(NULL, onset, true));
    duration_destroy(onset);
    duration_destroy(length);
}

int main(void) {
    test_rules_reference_framework();
    test_off_beat();
    test_syncopation_by_sustain();
    test_syncopation_by_rest();
    test_syncopation_by_accent();
    test_null_signature();
    printf("test_syncopation_rules (standard-meter): ok\n");
    return 0;
}

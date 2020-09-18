#include "time/time_signature.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static NoteValue *unit(int exponent) {
    NoteValue *value = NULL;
    assert(note_value_create(exponent, &value) == NOTE_VALUE_OK);
    return value;
}

static TimeSignature *make(unsigned beats, int exponent) {
    NoteValue *beat_unit = unit(exponent);
    TimeSignature *time_signature = NULL;
    assert(time_signature_create(beats, beat_unit, &time_signature) == TIME_SIGNATURE_OK);
    note_value_destroy(beat_unit);
    return time_signature;
}

static void test_create_rejects_invalid(void) {
    TimeSignature *time_signature = NULL;
    NoteValue *quarter = unit(2);

    assert(time_signature_create(4, NULL, &time_signature) == TIME_SIGNATURE_ERR_NULL_BOTTOM_UNIT);
    assert(time_signature_create(0, quarter, &time_signature) == TIME_SIGNATURE_ERR_TOP_NUMBER_OUT_OF_RANGE);
    assert(time_signature_create(TIME_SIGNATURE_MAX_TOP_NUMBER + 1, quarter, &time_signature) ==
           TIME_SIGNATURE_ERR_TOP_NUMBER_OUT_OF_RANGE);

    NoteValue *breve = unit(-1);
    NoteValue *thirty_second = unit(5);
    NoteValue *dotted_quarter = NULL;
    assert(note_value_create_dotted(2, 1, &dotted_quarter) == NOTE_VALUE_OK);
    assert(time_signature_create(4, breve, &time_signature) == TIME_SIGNATURE_ERR_BOTTOM_UNIT_OUT_OF_RANGE);
    assert(time_signature_create(4, thirty_second, &time_signature) == TIME_SIGNATURE_ERR_BOTTOM_UNIT_OUT_OF_RANGE);
    assert(time_signature_create(6, dotted_quarter, &time_signature) == TIME_SIGNATURE_ERR_BOTTOM_UNIT_DOTTED);
    assert(time_signature == NULL);

    note_value_destroy(quarter);
    note_value_destroy(breve);
    note_value_destroy(thirty_second);
    note_value_destroy(dotted_quarter);
}

/* The beat unit is read, not owned: it is still usable after create. */
static void test_create_does_not_take_beat_unit(void) {
    NoteValue *quarter = unit(2);
    TimeSignature *time_signature = NULL;
    assert(time_signature_create(3, quarter, &time_signature) == TIME_SIGNATURE_OK);
    assert(note_value_exponent(quarter) == 2);
    note_value_destroy(quarter);
    assert(time_signature_top_number(time_signature) == 3);
    time_signature_destroy(time_signature);
}

static void test_accessors_and_bottom_number(void) {
    unsigned numbers[] = {1, 2, 4, 8, 16};
    for (int exponent = TIME_SIGNATURE_MIN_BOTTOM_UNIT_EXPONENT; exponent <= TIME_SIGNATURE_MAX_BOTTOM_UNIT_EXPONENT;
         exponent++) {
        TimeSignature *time_signature = make(3, exponent);
        assert(time_signature_top_number(time_signature) == 3);
        assert(time_signature_bottom_unit_exponent(time_signature) == exponent);
        assert(time_signature_bottom_number(time_signature) == numbers[exponent]);
        time_signature_destroy(time_signature);
    }
}

/* Beat count and beat unit bounds are accepted. */
static void test_bounds(void) {
    TimeSignature *time_signature = make(1, 0);
    time_signature_destroy(time_signature);
    time_signature = make(TIME_SIGNATURE_MAX_TOP_NUMBER, 4);
    time_signature_destroy(time_signature);
}

static void assert_duration(const Duration *duration, long numerator, unsigned long denominator) {
    assert(duration_numerator(duration) == numerator);
    assert(duration_denominator(duration) == denominator);
}

static void test_beat_and_measure_duration(void) {
    TimeSignature *four_four = make(4, 2);
    Duration *beat = NULL;
    Duration *measure = NULL;
    assert(time_signature_unit_duration(four_four, &beat) == DURATION_OK);
    assert(time_signature_measure_duration(four_four, &measure) == DURATION_OK);
    assert_duration(beat, 1, 4);
    assert_duration(measure, 1, 1);
    duration_destroy(beat);
    duration_destroy(measure);
    time_signature_destroy(four_four);

    TimeSignature *three_four = make(3, 2);
    assert(time_signature_measure_duration(three_four, &measure) == DURATION_OK);
    assert_duration(measure, 3, 4);
    duration_destroy(measure);
    time_signature_destroy(three_four);

    TimeSignature *two_two = make(2, 1);
    assert(time_signature_measure_duration(two_two, &measure) == DURATION_OK);
    assert_duration(measure, 1, 1);
    duration_destroy(measure);
    time_signature_destroy(two_two);

    TimeSignature *three_eight = make(3, 3);
    assert(time_signature_measure_duration(three_eight, &measure) == DURATION_OK);
    assert_duration(measure, 3, 8);
    duration_destroy(measure);
    time_signature_destroy(three_eight);
}

/* Not a fraction: 2/4 and 4/8 hold the same length but are different. */
static void test_equality_is_not_fraction_equality(void) {
    TimeSignature *two_four = make(2, 2);
    TimeSignature *four_eight = make(4, 3);
    TimeSignature *also_two_four = make(2, 2);

    Duration *a = NULL;
    Duration *b = NULL;
    assert(time_signature_measure_duration(two_four, &a) == DURATION_OK);
    assert(time_signature_measure_duration(four_eight, &b) == DURATION_OK);
    assert(duration_equals(a, b));

    assert(!time_signature_equals(two_four, four_eight));
    assert(time_signature_equals(two_four, also_two_four));
    assert(!time_signature_equals(two_four, NULL));
    assert(!time_signature_equals(NULL, two_four));

    duration_destroy(a);
    duration_destroy(b);
    time_signature_destroy(two_four);
    time_signature_destroy(four_eight);
    time_signature_destroy(also_two_four);
}

int main(void) {
    test_create_rejects_invalid();
    test_create_does_not_take_beat_unit();
    test_accessors_and_bottom_number();
    test_bounds();
    test_beat_and_measure_duration();
    test_equality_is_not_fraction_equality();
    printf("test_time_signature: ok\n");
    return 0;
}

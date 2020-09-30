#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counting_rules.h"
#include "meter_test_util.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_counting_rule, standard_meter_counting_constraint,
                                    STANDARD_METER_COUNTING_RULE_COUNT);
}

static void assert_text(const TimeSignature *time_signature, long numerator, unsigned long denominator,
                        const char *expected, unsigned beat) {
    Duration *offset = make_duration(numerator, denominator);
    StandardMeterCount count;
    assert(standard_meter_count(time_signature, offset, &count));
    assert(count.beat == beat);
    char text[16];
    assert(standard_meter_count_text(&count, true, text, sizeof(text)));
    assert(strcmp(text, expected) == 0);
    duration_destroy(offset);
}

/* 4/4: 1 e + a 2 e + a 3 e + a 4 e + a, counted on sixteenths. */
static void test_four_four_sixteenths(void) {
    TimeSignature *four_four = make_signature(4, 2);
    const char *expected[] = {"1", "e", "+", "a", "2", "e", "+", "a", "3", "e", "+", "a", "4", "e", "+", "a"};
    for (long i = 0; i < 16; i++) {
        assert_text(four_four, i, 16, expected[i], (unsigned)(i / 4) + 1);
    }
    time_signature_destroy(four_four);
}

/* One level finer adds "ta" between the previous syllables. */
static void test_thirty_seconds_add_ta(void) {
    TimeSignature *four_four = make_signature(4, 2);
    const char *expected[] = {"1", "ta", "e", "ta", "+", "ta", "a", "ta"};
    for (long i = 0; i < 8; i++) {
        assert_text(four_four, i, 32, expected[i], 1);
    }
    assert_text(four_four, 8, 32, "2", 2);
    assert_text(four_four, 9, 32, "ta", 2);
    time_signature_destroy(four_four);
}

/* The same counting in 2/4 and 3/4, and in other beat units. */
static void test_other_signatures(void) {
    TimeSignature *three_four = make_signature(3, 2);
    assert_text(three_four, 1, 4, "2", 2);
    assert_text(three_four, 3, 8, "+", 2);
    assert_text(three_four, 1, 2, "3", 3);
    assert_text(three_four, 5, 8, "+", 3);
    time_signature_destroy(three_four);

    /* 4/2: the beat is a half note, so "+" is a quarter later */
    TimeSignature *four_two = make_signature(4, 1);
    assert_text(four_two, 1, 4, "+", 1);
    assert_text(four_two, 1, 2, "2", 2);
    assert_text(four_two, 3, 4, "+", 2);
    time_signature_destroy(four_two);

    /* 4/8 and 4/16 count the same way at their own beat */
    TimeSignature *four_eight = make_signature(4, 3);
    assert_text(four_eight, 1, 16, "+", 1);
    assert_text(four_eight, 1, 8, "2", 2);
    time_signature_destroy(four_eight);

    TimeSignature *four_sixteen = make_signature(4, 4);
    assert_text(four_sixteen, 1, 32, "+", 1);
    assert_text(four_sixteen, 1, 16, "2", 2);
    time_signature_destroy(four_sixteen);
}

static void test_count_declines(void) {
    TimeSignature *four_four = make_signature(4, 2);
    StandardMeterCount count = {99, 99, 99, NULL};

    Duration *end = make_duration(1, 1); /* at or past the end of the measure */
    Duration *past = make_duration(5, 4);
    Duration *too_fine = make_duration(1, 64); /* finer than an eighth of a beat */
    Duration *triplet = make_duration(1, 12);
    assert(!standard_meter_count(four_four, end, &count));
    assert(!standard_meter_count(four_four, past, &count));
    assert(!standard_meter_count(four_four, too_fine, &count));
    assert(!standard_meter_count(four_four, triplet, &count));
    assert(!standard_meter_count(NULL, end, &count));
    assert(!standard_meter_count(four_four, NULL, &count));
    assert(count.beat == 99);

    duration_destroy(end);
    duration_destroy(past);
    duration_destroy(too_fine);
    duration_destroy(triplet);
    time_signature_destroy(four_four);
}

/* Beats not articulated aloud are in parentheses. */
static void test_silent_beats_in_parentheses(void) {
    TimeSignature *four_four = make_signature(4, 2);
    Duration *offset = make_duration(1, 4);
    StandardMeterCount count;
    assert(standard_meter_count(four_four, offset, &count));

    char text[16];
    assert(standard_meter_count_text(&count, false, text, sizeof(text)));
    assert(strcmp(text, "(2)") == 0);

    Duration *and_position = make_duration(3, 8);
    assert(standard_meter_count(four_four, and_position, &count));
    assert(standard_meter_count_text(&count, false, text, sizeof(text)));
    assert(strcmp(text, "(+)") == 0);

    /* too small a buffer */
    char tiny[2];
    assert(!standard_meter_count_text(&count, false, tiny, sizeof(tiny)));
    assert(!standard_meter_count_text(NULL, true, text, sizeof(text)));

    duration_destroy(offset);
    duration_destroy(and_position);
    time_signature_destroy(four_four);
}

int main(void) {
    test_rules_reference_framework();
    test_four_four_sixteenths();
    test_thirty_seconds_add_ta();
    test_other_signatures();
    test_count_declines();
    test_silent_beats_in_parentheses();
    printf("test_counting_rules (standard-meter): ok\n");
    return 0;
}

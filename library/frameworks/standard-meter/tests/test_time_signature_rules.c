#include <assert.h>
#include <stdio.h>

#include "meter_test_util.h"
#include "rule_check.h"
#include "time_signature_rules.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_time_signature_rule, standard_meter_time_signature_constraint,
                                    STANDARD_METER_TIME_SIGNATURE_RULE_COUNT);
}

/* Top number 2, 3, or 4 is simple. */
static void test_simple_beats(void) {
    assert(!standard_meter_beats_are_simple(0));
    assert(!standard_meter_beats_are_simple(1));
    assert(standard_meter_beats_are_simple(2));
    assert(standard_meter_beats_are_simple(3));
    assert(standard_meter_beats_are_simple(4));
    assert(!standard_meter_beats_are_simple(5));
    assert(!standard_meter_beats_are_simple(6));
}

/* Bottom number: usually 2, 4, 8; 1 and 16 also occur. */
static void test_beat_unit(void) {
    bool usual[] = {false, true, true, true, false}; /* 1, 2, 4, 8, 16 */
    for (int exponent = 0; exponent <= 4; exponent++) {
        TimeSignature *time_signature = make_signature(3, exponent);
        assert(standard_meter_beat_unit_is_usual(time_signature) == usual[exponent]);
        assert(standard_meter_is_simple(time_signature)); /* 3/1 through 3/16 */
        time_signature_destroy(time_signature);
    }
    assert(!standard_meter_beat_unit_is_usual(NULL));
}

static void test_is_simple(void) {
    for (unsigned beats = 1; beats <= 8; beats++) {
        TimeSignature *time_signature = make_signature(beats, 2);
        assert(standard_meter_is_simple(time_signature) == (beats >= 2 && beats <= 4));
        time_signature_destroy(time_signature);
    }
    assert(!standard_meter_is_simple(NULL));
}

/* Common time is 4/4, cut time is 2/2. */
static void test_common_and_cut_time(void) {
    TimeSignature *four_four = make_signature(4, 2);
    TimeSignature *two_two = make_signature(2, 1);
    TimeSignature *two_four = make_signature(2, 2);
    TimeSignature *four_two = make_signature(4, 1);

    assert(standard_meter_signature_symbol(four_four) == TIME_SIGNATURE_SYMBOL_COMMON);
    assert(standard_meter_signature_symbol(two_two) == TIME_SIGNATURE_SYMBOL_CUT);
    assert(standard_meter_signature_symbol(two_four) == TIME_SIGNATURE_SYMBOL_NONE);
    assert(standard_meter_signature_symbol(four_two) == TIME_SIGNATURE_SYMBOL_NONE);
    assert(standard_meter_signature_symbol(NULL) == TIME_SIGNATURE_SYMBOL_NONE);

    TimeSignature *from_symbol = NULL;
    assert(standard_meter_symbol_signature(TIME_SIGNATURE_SYMBOL_COMMON, &from_symbol));
    assert(time_signature_equals(from_symbol, four_four));
    assert(standard_meter_is_simple(from_symbol)); /* simple quadruple */
    time_signature_destroy(from_symbol);

    assert(standard_meter_symbol_signature(TIME_SIGNATURE_SYMBOL_CUT, &from_symbol));
    assert(time_signature_equals(from_symbol, two_two));
    assert(standard_meter_grouping(time_signature_beats(from_symbol)) == METER_GROUPING_DUPLE);
    time_signature_destroy(from_symbol);

    from_symbol = NULL;
    assert(!standard_meter_symbol_signature(TIME_SIGNATURE_SYMBOL_NONE, &from_symbol));
    assert(from_symbol == NULL);

    time_signature_destroy(four_four);
    time_signature_destroy(two_two);
    time_signature_destroy(two_four);
    time_signature_destroy(four_two);
}

/* Not a fraction: 2/4 and 4/8 hold the same length but are not the same signature. */
static void test_not_a_fraction(void) {
    TimeSignature *two_four = make_signature(2, 2);
    TimeSignature *four_eight = make_signature(4, 3);
    TimeSignature *three_four = make_signature(3, 2);

    assert(standard_meter_same_measure_length(two_four, four_eight));
    assert(!time_signature_equals(two_four, four_eight));
    assert(!standard_meter_same_measure_length(two_four, three_four));
    assert(!standard_meter_same_measure_length(two_four, NULL));

    time_signature_destroy(two_four);
    time_signature_destroy(four_eight);
    time_signature_destroy(three_four);
}

int main(void) {
    test_rules_reference_framework();
    test_simple_beats();
    test_beat_unit();
    test_is_simple();
    test_common_and_cut_time();
    test_not_a_fraction();
    printf("test_time_signature_rules (standard-meter): ok\n");
    return 0;
}

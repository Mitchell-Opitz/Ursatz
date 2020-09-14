#include <assert.h>
#include <stdio.h>

#include "beat_tie_rules.h"
#include "meter_test_util.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_beat_tie_rule, standard_meter_beat_tie_constraint,
                                    STANDARD_METER_BEAT_TIE_RULE_COUNT);
}

/* Returns the first part's length as n/d via the out params, or false when not split. */
static bool split(const TimeSignature *time_signature, long start_n, unsigned long start_d, long length_n,
                  unsigned long length_d, long *first_n, unsigned long *first_d) {
    Duration *start = make_duration(start_n, start_d);
    Duration *length = make_duration(length_n, length_d);
    Duration *first = NULL;
    bool result = standard_meter_beat_split(time_signature, start, length, &first);
    if (result) {
        *first_n = duration_numerator(first);
        *first_d = duration_denominator(first);
        duration_destroy(first);
    } else {
        assert(first == NULL);
    }
    duration_destroy(start);
    duration_destroy(length);
    return result;
}

/* A quarter note starting on the "+" of beat 1 buries beat 2; split into two eighths. */
static void test_split_across_beat(void) {
    TimeSignature *four_four = make_signature(4, 2);
    long n = 0;
    unsigned long d = 0;

    assert(split(four_four, 1, 8, 1, 4, &n, &d));
    assert(n == 1 && d == 8);

    /* a dotted eighth + sixteenth shape: eighth-plus-sixteenth starting on the "e" */
    assert(split(four_four, 1, 16, 1, 4, &n, &d));
    assert(n == 3 && d == 16);

    /* the same note later in the measure splits at its own next beat */
    assert(split(four_four, 5, 8, 1, 4, &n, &d));
    assert(n == 1 && d == 8);
    time_signature_destroy(four_four);

    /* an eighth-note beat in 6/8-as-written (six eighth beats) */
    TimeSignature *six_eight = make_signature(6, 3);
    assert(split(six_eight, 1, 16, 1, 8, &n, &d));
    assert(n == 1 && d == 16);
    time_signature_destroy(six_eight);
}

/* Nothing to split: starts on a beat, or ends by the next boundary. */
static void test_no_split(void) {
    TimeSignature *four_four = make_signature(4, 2);
    long n = 0;
    unsigned long d = 0;

    assert(!split(four_four, 0, 1, 1, 2, &n, &d));  /* half note on beat 1 */
    assert(!split(four_four, 1, 4, 1, 2, &n, &d));  /* half note on beat 2 */
    assert(!split(four_four, 0, 1, 1, 1, &n, &d));  /* whole note */
    assert(!split(four_four, 1, 8, 1, 8, &n, &d));  /* eighth ending exactly on beat 2 */
    assert(!split(four_four, 1, 8, 1, 16, &n, &d)); /* sixteenth inside the beat */
    time_signature_destroy(four_four);
}

static void test_split_declines(void) {
    TimeSignature *four_four = make_signature(4, 2);
    long n = 0;
    unsigned long d = 0;

    assert(!split(four_four, 1, 1, 1, 8, &n, &d));  /* start outside the measure */
    assert(!split(four_four, 1, 8, 0, 1, &n, &d));  /* empty note */
    assert(!standard_meter_beat_split(NULL, NULL, NULL, NULL));
    time_signature_destroy(four_four);
}

int main(void) {
    test_rules_reference_framework();
    test_split_across_beat();
    test_no_split();
    test_split_declines();
    printf("test_beat_tie_rules (standard-meter): ok\n");
    return 0;
}

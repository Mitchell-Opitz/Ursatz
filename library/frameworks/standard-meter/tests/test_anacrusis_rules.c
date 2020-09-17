#include <assert.h>
#include <stdio.h>

#include "anacrusis_rules.h"
#include "meter_test_util.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_anacrusis_rule, standard_meter_anacrusis_constraint,
                                    STANDARD_METER_ANACRUSIS_RULE_COUNT);
}

/* A pickup is shorter than a measure and not empty. */
static void test_pickup_valid(void) {
    TimeSignature *four_four = make_signature(4, 2);
    Duration *quarter = make_duration(1, 4);
    Duration *three_quarters = make_duration(3, 4);
    Duration *measure = make_duration(1, 1);
    Duration *longer = make_duration(5, 4);
    Duration *empty = make_duration(0, 1);

    assert(standard_meter_pickup_valid(four_four, quarter));
    assert(standard_meter_pickup_valid(four_four, three_quarters));
    assert(!standard_meter_pickup_valid(four_four, measure));
    assert(!standard_meter_pickup_valid(four_four, longer));
    assert(!standard_meter_pickup_valid(four_four, empty));
    assert(!standard_meter_pickup_valid(four_four, NULL));
    assert(!standard_meter_pickup_valid(NULL, quarter));

    duration_destroy(quarter);
    duration_destroy(three_quarters);
    duration_destroy(measure);
    duration_destroy(longer);
    duration_destroy(empty);
    time_signature_destroy(four_four);
}

/* A quarter-note pickup in 4/4 is counted as beat 4: it starts 3/4 in. */
static void test_pickup_position(void) {
    TimeSignature *four_four = make_signature(4, 2);
    Duration *quarter = make_duration(1, 4);
    Duration *eighth = make_duration(1, 8);
    Duration *offset = NULL;

    assert(standard_meter_pickup_start_offset(four_four, quarter, &offset));
    assert_duration(offset, 3, 4);
    duration_destroy(offset);

    assert(standard_meter_pickup_start_offset(four_four, eighth, &offset));
    assert_duration(offset, 7, 8);
    duration_destroy(offset);

    offset = NULL;
    Duration *measure = make_duration(1, 1);
    assert(!standard_meter_pickup_start_offset(four_four, measure, &offset));
    assert(offset == NULL);

    duration_destroy(measure);
    duration_destroy(quarter);
    duration_destroy(eighth);
    time_signature_destroy(four_four);
}

/* The last measure is shorter by the pickup: three beats in 4/4 after a quarter pickup. */
static void test_final_measure(void) {
    TimeSignature *four_four = make_signature(4, 2);
    TimeSignature *three_four = make_signature(3, 2);
    Duration *quarter = make_duration(1, 4);
    Duration *final = NULL;

    assert(standard_meter_final_measure_duration(four_four, quarter, &final));
    assert_duration(final, 3, 4);
    duration_destroy(final);

    assert(standard_meter_final_measure_duration(three_four, quarter, &final));
    assert_duration(final, 1, 2);
    duration_destroy(final);

    /* pickup plus final measure is one full measure */
    Duration *pickup_and_final = NULL;
    assert(standard_meter_final_measure_duration(four_four, quarter, &final));
    assert(duration_add(quarter, final, &pickup_and_final) == DURATION_OK);
    assert_duration(pickup_and_final, 1, 1);
    duration_destroy(pickup_and_final);
    duration_destroy(final);

    final = NULL;
    Duration *too_long = make_duration(1, 1);
    assert(!standard_meter_final_measure_duration(four_four, too_long, &final));
    assert(final == NULL);

    duration_destroy(too_long);
    duration_destroy(quarter);
    time_signature_destroy(four_four);
    time_signature_destroy(three_four);
}

int main(void) {
    test_rules_reference_framework();
    test_pickup_valid();
    test_pickup_position();
    test_final_measure();
    printf("test_anacrusis_rules (standard-meter): ok\n");
    return 0;
}

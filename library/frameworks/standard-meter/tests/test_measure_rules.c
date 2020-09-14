#include <assert.h>
#include <stdio.h>

#include "measure_rules.h"
#include "meter_test_util.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_measure_rule, standard_meter_measure_constraint,
                                    STANDARD_METER_MEASURE_RULE_COUNT);
}

/* A measure is one beat group: beats times the beat unit. */
static void test_measure_is_one_beat_group(void) {
    TimeSignature *duple = make_signature(2, 2);
    TimeSignature *triple = make_signature(3, 2);
    TimeSignature *quadruple = make_signature(4, 2);
    TimeSignature *irregular = make_signature(5, 2);
    Duration *length = NULL;

    assert(standard_meter_measure_grouping(duple) == METER_GROUPING_DUPLE);
    assert(standard_meter_measure_grouping(triple) == METER_GROUPING_TRIPLE);
    assert(standard_meter_measure_grouping(quadruple) == METER_GROUPING_QUADRUPLE);
    assert(standard_meter_measure_grouping(irregular) == METER_GROUPING_NONE);
    assert(standard_meter_measure_grouping(NULL) == METER_GROUPING_NONE);

    assert(standard_meter_measure_duration(duple, &length));
    assert_duration(length, 1, 2);
    duration_destroy(length);
    assert(standard_meter_measure_duration(triple, &length));
    assert_duration(length, 3, 4);
    duration_destroy(length);
    assert(standard_meter_measure_duration(quadruple, &length));
    assert_duration(length, 1, 1);
    duration_destroy(length);

    length = NULL;
    assert(!standard_meter_measure_duration(NULL, &length));
    assert(length == NULL);

    time_signature_destroy(duple);
    time_signature_destroy(triple);
    time_signature_destroy(quadruple);
    time_signature_destroy(irregular);
}

/* A rest of n measures lasts n times the measure. */
static void test_multi_measure_rest(void) {
    TimeSignature *four_four = make_signature(4, 2);
    TimeSignature *three_four = make_signature(3, 2);
    Duration *length = NULL;

    assert(standard_meter_multi_measure_rest_duration(four_four, 4, &length));
    assert_duration(length, 4, 1);
    duration_destroy(length);

    assert(standard_meter_multi_measure_rest_duration(three_four, 4, &length));
    assert_duration(length, 3, 1);
    duration_destroy(length);

    assert(standard_meter_multi_measure_rest_duration(three_four, 1, &length));
    assert_duration(length, 3, 4);
    duration_destroy(length);

    length = NULL;
    assert(!standard_meter_multi_measure_rest_duration(four_four, 0, &length));
    assert(!standard_meter_multi_measure_rest_duration(NULL, 4, &length));
    assert(length == NULL);

    time_signature_destroy(four_four);
    time_signature_destroy(three_four);
}

int main(void) {
    test_rules_reference_framework();
    test_measure_is_one_beat_group();
    test_multi_measure_rest();
    printf("test_measure_rules (standard-meter): ok\n");
    return 0;
}

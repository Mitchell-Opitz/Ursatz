#include <assert.h>
#include <stdio.h>

#include "hypermeter_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_hypermeter_rule, standard_meter_hypermeter_constraint,
                                    STANDARD_METER_HYPERMETER_RULE_COUNT);
}

/* Measures are numbered like beats and wrap at the group size. */
static void test_count_wraps(void) {
    unsigned count = 0;
    unsigned duple[] = {1, 2, 1, 2, 1};
    unsigned triple[] = {1, 2, 3, 1, 2, 3, 1};
    unsigned quadruple[] = {1, 2, 3, 4, 1, 2, 3, 4, 1};

    for (unsigned i = 0; i < 5; i++) {
        assert(standard_meter_hypermetric_count(i, METER_GROUPING_DUPLE, &count) && count == duple[i]);
    }
    for (unsigned i = 0; i < 7; i++) {
        assert(standard_meter_hypermetric_count(i, METER_GROUPING_TRIPLE, &count) && count == triple[i]);
    }
    for (unsigned i = 0; i < 9; i++) {
        assert(standard_meter_hypermetric_count(i, METER_GROUPING_QUADRUPLE, &count) && count == quadruple[i]);
    }

    count = 77;
    assert(!standard_meter_hypermetric_count(0, METER_GROUPING_NONE, &count));
    assert(count == 77);
}

/* Eight measures of a quadruple hypermeter: 1 and 3 are accented, 2 and 4 are not. */
static void test_eight_measures_quadruple(void) {
    bool accented[] = {true, false, true, false, true, false, true, false};
    for (unsigned i = 0; i < 8; i++) {
        unsigned count;
        assert(standard_meter_hypermetric_count(i, METER_GROUPING_QUADRUPLE, &count));
        assert(standard_meter_hypermetric_accented(count, METER_GROUPING_QUADRUPLE) == accented[i]);
    }
}

/* Duple and triple accent only the first measure. */
static void test_accent_in_duple_and_triple(void) {
    assert(standard_meter_hypermetric_accented(1, METER_GROUPING_DUPLE));
    assert(!standard_meter_hypermetric_accented(2, METER_GROUPING_DUPLE));

    assert(standard_meter_hypermetric_accented(1, METER_GROUPING_TRIPLE));
    assert(!standard_meter_hypermetric_accented(2, METER_GROUPING_TRIPLE));
    assert(!standard_meter_hypermetric_accented(3, METER_GROUPING_TRIPLE));
}

static void test_accent_declines(void) {
    assert(!standard_meter_hypermetric_accented(0, METER_GROUPING_QUADRUPLE));
    assert(!standard_meter_hypermetric_accented(5, METER_GROUPING_QUADRUPLE));
    assert(!standard_meter_hypermetric_accented(3, METER_GROUPING_DUPLE)); /* past the group size */
    assert(!standard_meter_hypermetric_accented(1, METER_GROUPING_NONE));
}

int main(void) {
    test_rules_reference_framework();
    test_count_wraps();
    test_eight_measures_quadruple();
    test_accent_in_duple_and_triple();
    test_accent_declines();
    printf("test_hypermeter_rules (standard-meter): ok\n");
    return 0;
}

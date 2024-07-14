#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "hypermeter_rules.h"
#include "rule_check.h"

static bool near(double a, double b) {
    return fabs(a - b) < 1e-12;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_hypermeter_rule, standard_meter_hypermeter_constraint,
                                    STANDARD_METER_HYPERMETER_RULE_COUNT);
}

static void test_score(void) {
    double score = -1.0;
    /* four measures with the cadence in the last: both lengths fill and hold it */
    assert(standard_meter_hypermeasure_score(4, 3, false, 4, &score) && near(score, 1.0));
    assert(standard_meter_hypermeasure_score(4, 3, false, 2, &score) && near(score, 1.0));
    /* a cadence in the second measure: the only hypermeasure of four holds it, the last of two does not */
    assert(standard_meter_hypermeasure_score(4, 1, false, 4, &score) && near(score, 1.0));
    assert(standard_meter_hypermeasure_score(4, 1, false, 2, &score) && near(score, 0.5));
    /* a length that does not fill the phrase */
    assert(standard_meter_hypermeasure_score(6, 5, false, 4, &score) && near(score, (4.0 / 6.0 + 1.0) / 2.0));
    assert(standard_meter_hypermeasure_score(2, 1, false, 4, &score) && near(score, 0.5));
    /* no cadence: the fill alone */
    assert(standard_meter_hypermeasure_score(6, -1, false, 4, &score) && near(score, 4.0 / 6.0));
    /* an elided cadence: whole hypermeasures put the next phrase on a hyperdownbeat */
    assert(standard_meter_hypermeasure_score(4, -1, true, 4, &score) && near(score, 1.0));
    assert(standard_meter_hypermeasure_score(6, -1, true, 4, &score) && near(score, (4.0 / 6.0 + 0.5) / 2.0));
}

static void test_score_refuses(void) {
    double score = 7.0;
    assert(!standard_meter_hypermeasure_score(4, 3, false, 3, &score));
    assert(!standard_meter_hypermeasure_score(4, 3, false, 0, &score));
    assert(!standard_meter_hypermeasure_score(0, -1, false, 4, &score));
    assert(!standard_meter_hypermeasure_score(4, 4, false, 4, &score)); /* past the end, and not elided */
    assert(!standard_meter_hypermeasure_score(4, -2, false, 4, &score));
    assert(!standard_meter_hypermeasure_score(4, 3, false, 4, NULL));
    assert(score == 7.0);
}

static void test_choose(void) {
    unsigned length = 0;
    double score = 0.0;
    assert(standard_meter_hypermeasure_choose(4, 3, false, &length, &score) && length == 4 && near(score, 1.0)); /* tie: longer */
    assert(standard_meter_hypermeasure_choose(2, 1, false, &length, &score) && length == 2 && near(score, 1.0));
    assert(standard_meter_hypermeasure_choose(6, 5, false, &length, &score) && length == 2 && near(score, 1.0));
    assert(standard_meter_hypermeasure_choose(8, 7, false, &length, &score) && length == 4);
    assert(standard_meter_hypermeasure_choose(3, 2, false, &length, &score) && length == 2);
    assert(!standard_meter_hypermeasure_choose(0, -1, false, &length, &score));
    assert(!standard_meter_hypermeasure_choose(4, 3, false, NULL, &score));
}

static void test_expansion_and_elision(void) {
    unsigned length = 0;
    assert(standard_meter_hypermeasure_extended_length(4, 1, &length) && length == 5);
    assert(standard_meter_hypermeasure_extended_length(4, -1, &length) && length == 3);
    assert(!standard_meter_hypermeasure_extended_length(1, -1, &length));
    assert(!standard_meter_hypermeasure_extended_length(0, 1, &length));
    assert(!standard_meter_hypermeasure_extended_length(4, 1, NULL));

    unsigned ending = 0;
    unsigned next = 0;
    assert(standard_meter_hypermetric_elision(4, &ending, &next) && ending == 4 && next == 1);
    assert(standard_meter_hypermetric_elision(5, &ending, &next) && ending == 5 && next == 1);
    assert(!standard_meter_hypermetric_elision(0, &ending, &next));
    assert(!standard_meter_hypermetric_elision(4, NULL, &next));
}

int main(void) {
    test_rules_reference_framework();
    test_score();
    test_score_refuses();
    test_choose();
    test_expansion_and_elision();
    printf("test_hypermeasure_rules (standard-meter): ok\n");
    return 0;
}

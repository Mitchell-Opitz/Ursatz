#include <assert.h>
#include <stdio.h>

#include "grouping_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_grouping_rule, standard_meter_grouping_constraint,
                                    STANDARD_METER_GROUPING_RULE_COUNT);
}

/* The beat divides into two, then four. */
static void test_beat_division(void) {
    unsigned parts = 0;
    assert(standard_meter_beat_division_parts(0, &parts) && parts == 1);
    assert(standard_meter_beat_division_parts(1, &parts) && parts == 2);
    assert(standard_meter_beat_division_parts(2, &parts) && parts == 4);
    assert(standard_meter_beat_division_parts(STANDARD_METER_MAX_DIVISION_DEPTH, &parts) && parts == 64);

    parts = 7;
    assert(!standard_meter_beat_division_parts(STANDARD_METER_MAX_DIVISION_DEPTH + 1, &parts));
    assert(parts == 7);
}

static void test_grouping(void) {
    assert(standard_meter_grouping(2) == METER_GROUPING_DUPLE);
    assert(standard_meter_grouping(3) == METER_GROUPING_TRIPLE);
    assert(standard_meter_grouping(4) == METER_GROUPING_QUADRUPLE);
    assert(standard_meter_grouping(0) == METER_GROUPING_NONE);
    assert(standard_meter_grouping(1) == METER_GROUPING_NONE);
    assert(standard_meter_grouping(5) == METER_GROUPING_NONE);
}

/* Beat 1 is the downbeat, the last beat the upbeat. */
static void test_beat_roles(void) {
    BeatRole role;

    assert(standard_meter_beat_role(1, 2, &role) && role == BEAT_ROLE_DOWNBEAT);
    assert(standard_meter_beat_role(2, 2, &role) && role == BEAT_ROLE_UPBEAT);

    assert(standard_meter_beat_role(1, 3, &role) && role == BEAT_ROLE_DOWNBEAT);
    assert(standard_meter_beat_role(2, 3, &role) && role == BEAT_ROLE_MIDDLE);
    assert(standard_meter_beat_role(3, 3, &role) && role == BEAT_ROLE_UPBEAT);

    assert(standard_meter_beat_role(1, 4, &role) && role == BEAT_ROLE_DOWNBEAT);
    assert(standard_meter_beat_role(2, 4, &role) && role == BEAT_ROLE_MIDDLE);
    assert(standard_meter_beat_role(3, 4, &role) && role == BEAT_ROLE_MIDDLE);
    assert(standard_meter_beat_role(4, 4, &role) && role == BEAT_ROLE_UPBEAT);

    /* a one-beat group's only beat is its downbeat */
    assert(standard_meter_beat_role(1, 1, &role) && role == BEAT_ROLE_DOWNBEAT);

    role = BEAT_ROLE_MIDDLE;
    assert(!standard_meter_beat_role(0, 4, &role));
    assert(!standard_meter_beat_role(5, 4, &role));
    assert(role == BEAT_ROLE_MIDDLE);
}

int main(void) {
    test_rules_reference_framework();
    test_beat_division();
    test_grouping();
    test_beat_roles();
    printf("test_grouping_rules (standard-meter): ok\n");
    return 0;
}

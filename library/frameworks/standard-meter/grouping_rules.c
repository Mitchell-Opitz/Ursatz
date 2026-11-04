#include "grouping_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_GROUPING_RULE_COUNT] = {"grouping-beat-division", "grouping-grouping",
                                                                       "grouping-beat-roles"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_meter_framework, "standard-meter",
    NAMES, STANDARD_METER_GROUPING_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_meter_grouping, StandardMeterGroupingRule, STANDARD_METER_GROUPING_RULE_COUNT, g_table)

bool standard_meter_beat_division_parts(unsigned depth, unsigned *out_parts) {
    if (depth > STANDARD_METER_MAX_DIVISION_DEPTH) {
        return false;
    }
    *out_parts = 1u << depth;
    return true;
}

MeterGrouping standard_meter_grouping(unsigned beats) {
    switch (beats) {
    case 2:
        return METER_GROUPING_DUPLE;
    case 3:
        return METER_GROUPING_TRIPLE;
    case 4:
        return METER_GROUPING_QUADRUPLE;
    default:
        return METER_GROUPING_NONE;
    }
}

bool standard_meter_beat_role(unsigned beat, unsigned beats, BeatRole *out) {
    if (beat == 0 || beat > beats) {
        return false;
    }
    if (beat == 1) {
        *out = BEAT_ROLE_DOWNBEAT;
    } else if (beat == beats) {
        *out = BEAT_ROLE_UPBEAT;
    } else {
        *out = BEAT_ROLE_MIDDLE;
    }
    return true;
}

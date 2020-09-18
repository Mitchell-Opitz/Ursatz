#include "measure_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_MEASURE_RULE_COUNT] = {"measure-group", "measure-multi-measure-rest"};

static StandardMeterRulePair g_pairs[STANDARD_METER_MEASURE_RULE_COUNT];
static bool g_built[STANDARD_METER_MEASURE_RULE_COUNT];

static bool valid_rule(StandardMeterMeasureRule which) {
    return which >= 0 && which < STANDARD_METER_MEASURE_RULE_COUNT;
}

static void ensure_built(StandardMeterMeasureRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_meter_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_meter_measure_rule(StandardMeterMeasureRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_meter_measure_constraint(StandardMeterMeasureRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_meter_measure_duration(const TimeSignature *time_signature, Duration **out) {
    if (time_signature == NULL) {
        return false;
    }
    return time_signature_measure_duration(time_signature, out) == DURATION_OK;
}

MeterGrouping standard_meter_measure_grouping(const TimeSignature *time_signature) {
    if (time_signature == NULL) {
        return METER_GROUPING_NONE;
    }
    return standard_meter_grouping(time_signature_top_number(time_signature));
}

bool standard_meter_multi_measure_rest_duration(const TimeSignature *time_signature, unsigned measures,
                                                Duration **out) {
    if (time_signature == NULL || measures == 0) {
        return false;
    }
    Duration *measure = NULL;
    if (time_signature_measure_duration(time_signature, &measure) != DURATION_OK) {
        return false;
    }
    DurationResult result = duration_create(duration_numerator(measure) * (long)measures,
                                            duration_denominator(measure), out);
    duration_destroy(measure);
    return result == DURATION_OK;
}

#include "measure_rules.h"

#include "meter_class_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_MEASURE_RULE_COUNT] = {"measure-group", "measure-multi-measure-rest"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_meter_framework, "standard-meter",
    NAMES, STANDARD_METER_MEASURE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_meter_measure, StandardMeterMeasureRule, STANDARD_METER_MEASURE_RULE_COUNT, g_table)

bool standard_meter_measure_duration(const TimeSignature *time_signature, Duration **out) {
    if (time_signature == NULL) {
        return false;
    }
    return time_signature_measure_duration(time_signature, out) == DURATION_OK;
}

MeterGrouping standard_meter_measure_grouping(const TimeSignature *time_signature) {
    return standard_meter_classify(time_signature).grouping;
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

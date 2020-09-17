#include "anacrusis_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_ANACRUSIS_RULE_COUNT] = {"anacrusis-pickup-valid",
                                                                        "anacrusis-pickup-position",
                                                                        "anacrusis-final-measure"};

static StandardMeterRulePair g_pairs[STANDARD_METER_ANACRUSIS_RULE_COUNT];
static bool g_built[STANDARD_METER_ANACRUSIS_RULE_COUNT];

static bool valid_rule(StandardMeterAnacrusisRule which) {
    return which >= 0 && which < STANDARD_METER_ANACRUSIS_RULE_COUNT;
}

static void ensure_built(StandardMeterAnacrusisRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_meter_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_meter_anacrusis_rule(StandardMeterAnacrusisRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_meter_anacrusis_constraint(StandardMeterAnacrusisRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

/* measure length minus pickup length, only when the pickup is valid. */
static bool measure_minus_pickup(const TimeSignature *time_signature, const Duration *pickup, Duration **out) {
    if (time_signature == NULL || pickup == NULL || duration_numerator(pickup) == 0) {
        return false;
    }
    Duration *measure = NULL;
    if (time_signature_measure_duration(time_signature, &measure) != DURATION_OK) {
        return false;
    }

    /* pickup < measure, by cross-multiplying the reduced fractions */
    bool shorter = duration_numerator(pickup) * (long)duration_denominator(measure) <
                   duration_numerator(measure) * (long)duration_denominator(pickup);
    bool ok = shorter && duration_subtract(measure, pickup, out) == DURATION_OK;
    duration_destroy(measure);
    return ok;
}

bool standard_meter_pickup_valid(const TimeSignature *time_signature, const Duration *pickup) {
    Duration *remainder = NULL;
    if (!measure_minus_pickup(time_signature, pickup, &remainder)) {
        return false;
    }
    duration_destroy(remainder);
    return true;
}

bool standard_meter_pickup_start_offset(const TimeSignature *time_signature, const Duration *pickup,
                                        Duration **out) {
    return measure_minus_pickup(time_signature, pickup, out);
}

bool standard_meter_final_measure_duration(const TimeSignature *time_signature, const Duration *pickup,
                                           Duration **out) {
    return measure_minus_pickup(time_signature, pickup, out);
}

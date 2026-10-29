#include "hypermeter_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_HYPERMETER_RULE_COUNT] = {"hypermeter-count-measures",
                                                                         "hypermeter-accent", "hypermeter-length",
                                                                         "hypermeter-expansion", "hypermeter-elision"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_meter_framework, "standard-meter",
    NAMES, STANDARD_METER_HYPERMETER_RULE_COUNT);

static bool valid_rule(StandardMeterHypermeterRule which) {
    return which >= 0 && which < STANDARD_METER_HYPERMETER_RULE_COUNT;
}

const Rule *standard_meter_hypermeter_rule(StandardMeterHypermeterRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_meter_hypermeter_constraint(StandardMeterHypermeterRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

/* Measures per hypermetric group, or 0 for no grouping. */
static unsigned group_size(MeterGrouping grouping) {
    switch (grouping) {
    case METER_GROUPING_DUPLE:
        return 2;
    case METER_GROUPING_TRIPLE:
        return 3;
    case METER_GROUPING_QUADRUPLE:
        return 4;
    default:
        return 0;
    }
}

bool standard_meter_hypermetric_count(unsigned measure_index, MeterGrouping grouping, unsigned *out_count) {
    unsigned size = group_size(grouping);
    if (size == 0) {
        return false;
    }
    *out_count = measure_index % size + 1;
    return true;
}

bool standard_meter_hypermetric_accented(unsigned count, MeterGrouping grouping) {
    unsigned size = group_size(grouping);
    if (size == 0 || count < 1 || count > size) {
        return false;
    }
    return count == 1 || (grouping == METER_GROUPING_QUADRUPLE && count == 3);
}

bool standard_meter_hypermeasure_score(unsigned phrase_measures, int arrival_index, bool elided, unsigned length,
                                       double *out_score) {
    if (out_score == NULL || (length != 2 && length != 4) || phrase_measures == 0 || arrival_index < -1 ||
        (!elided && arrival_index >= (int)phrase_measures)) {
        return false;
    }
    double fill = (double)((phrase_measures / length) * length) / (double)phrase_measures;
    if (arrival_index < 0 && !elided) {
        *out_score = fill;
        return true;
    }
    double cadence;
    if (elided) {
        cadence = phrase_measures % length == 0 ? 1.0 : 0.5;
    } else {
        unsigned last_start = ((phrase_measures - 1) / length) * length;
        cadence = (unsigned)arrival_index >= last_start ? 1.0 : 0.0;
    }
    *out_score = (fill + cadence) / 2.0;
    return true;
}

bool standard_meter_hypermeasure_choose(unsigned phrase_measures, int arrival_index, bool elided, unsigned *out_length,
                                        double *out_score) {
    double four = 0.0;
    double two = 0.0;
    if (out_length == NULL || out_score == NULL ||
        !standard_meter_hypermeasure_score(phrase_measures, arrival_index, elided, 4, &four) ||
        !standard_meter_hypermeasure_score(phrase_measures, arrival_index, elided, 2, &two)) {
        return false;
    }
    if (four >= two) {
        *out_length = 4;
        *out_score = four;
    } else {
        *out_length = 2;
        *out_score = two;
    }
    return true;
}

bool standard_meter_hypermeasure_extended_length(unsigned base_length, int added_measures, unsigned *out_length) {
    long result = (long)base_length + (long)added_measures;
    if (out_length == NULL || base_length == 0 || result < 1) {
        return false;
    }
    *out_length = (unsigned)result;
    return true;
}

bool standard_meter_hypermetric_elision(unsigned ending_length, unsigned *out_count_in_ending, unsigned *out_count_in_next) {
    if (ending_length == 0 || out_count_in_ending == NULL || out_count_in_next == NULL) {
        return false;
    }
    *out_count_in_ending = ending_length;
    *out_count_in_next = 1;
    return true;
}

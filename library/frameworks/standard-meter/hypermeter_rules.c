#include "hypermeter_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_HYPERMETER_RULE_COUNT] = {"hypermeter-count-measures",
                                                                         "hypermeter-accent"};

static StandardMeterRulePair g_pairs[STANDARD_METER_HYPERMETER_RULE_COUNT];
static bool g_built[STANDARD_METER_HYPERMETER_RULE_COUNT];

static bool valid_rule(StandardMeterHypermeterRule which) {
    return which >= 0 && which < STANDARD_METER_HYPERMETER_RULE_COUNT;
}

static void ensure_built(StandardMeterHypermeterRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_meter_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_meter_hypermeter_rule(StandardMeterHypermeterRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_meter_hypermeter_constraint(StandardMeterHypermeterRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
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

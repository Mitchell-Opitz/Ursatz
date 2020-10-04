#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_HYPERMETER_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_HYPERMETER_RULES_H

#include <stdbool.h>

#include "grouping_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Hypermeter rules for standard-meter-v1. Hypermeter is meter one level
 * up: groups of measures that form patterns of accentuation, especially
 * at faster tempos, where each measure is felt as a beat of a larger
 * group:
 *
 *   COUNT   the measures of a hypermetric group are numbered like beats,
 *           1 up to the group size (2, 3, or 4 measures: duple, triple,
 *           quadruple hypermeter), then start again
 *   ACCENT  the first measure of a group is accented; in quadruple
 *           hypermeter the third is also accented, more than the second
 *           and fourth. (Beethoven's Ninth, "Scherzo", counted in a
 *           quadruple pattern: measures 1 and 3 are more accented.)
 *
 * Hypermeter here is pure counting on a measure index: no measure
 * entities, and a pickup measure is the caller's to account for by
 * shifting its index. A piece's hypermeter can change or be ambiguous;
 * this states the counting, not how to hear it.
 */

typedef enum {
    STANDARD_METER_HYPERMETER_RULE_COUNT_MEASURES = 0,
    STANDARD_METER_HYPERMETER_RULE_ACCENT,
    STANDARD_METER_HYPERMETER_RULE_COUNT
} StandardMeterHypermeterRule;

const Rule *standard_meter_hypermeter_rule(StandardMeterHypermeterRule which);
const Constraint *standard_meter_hypermeter_constraint(StandardMeterHypermeterRule which);

/*
 * COUNT: the hypermetric count (1 to the group size) of the measure at
 * `measure_index` (0-based) in a hypermeter of the given grouping. False,
 * leaving *out_count untouched, for METER_GROUPING_NONE.
 */
bool standard_meter_hypermetric_count(unsigned measure_index, MeterGrouping grouping, unsigned *out_count);

/* ACCENT: true when hypermetric count `count` is accented in the grouping:
   count 1 always, and count 3 in quadruple. False for any other count,
   for a count outside 1 to the group size, and for METER_GROUPING_NONE. */
bool standard_meter_hypermetric_accented(unsigned count, MeterGrouping grouping);

#endif

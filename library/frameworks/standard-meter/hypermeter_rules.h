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
 *   LENGTH     a phrase is heard in hypermeasures of 2 or 4 measures. The length that suits a phrase is the one
 *              whose hypermeasures fill it whole, and that holds the cadence's arrival in the last of them:
 *              the score of a length is the mean of those two fits (each 0 to 1), and the best score wins,
 *              the longer length on a tie
 *   EXPANSION  an expansion changes a hypermeasure rather than breaking the grouping: an internal expansion
 *              adds hyperbeats to the hypermeasure it falls in, an external one (an extension after the
 *              cadence) adds a hypermeasure of its own, and a contraction takes hyperbeats from the last
 *   ELISION    an elided cadence (the arrival measure is also the next phrase's first measure) is counted
 *              twice: the last hyperbeat of the ending hypermeasure is the first hyperbeat of the next
 *              ("4 = 1" after a 4-measure hypermeasure)
 *
 * Hypermeter here is pure counting on a measure index: no measure
 * entities, and a pickup measure is the caller's to account for by
 * shifting its index. A piece's hypermeter can change or be ambiguous;
 * this states the counting, not how to hear it.
 */

typedef enum {
    STANDARD_METER_HYPERMETER_RULE_COUNT_MEASURES = 0,
    STANDARD_METER_HYPERMETER_RULE_ACCENT,
    STANDARD_METER_HYPERMETER_RULE_LENGTH,
    STANDARD_METER_HYPERMETER_RULE_EXPANSION,
    STANDARD_METER_HYPERMETER_RULE_ELISION,
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

/*
 * LENGTH: the score (0 to 1) of hearing a phrase of `phrase_measures` measures in hypermeasures of `length`
 * measures (2 or 4). `arrival_index` is the 0-based measure of the phrase the cadence arrives in, or -1 for
 * none; `elided` says the arrival is the next phrase's first measure (so it is one past the phrase). The fits:
 *   fill     the share of the phrase's measures that whole hypermeasures cover
 *   cadence  1 when the arrival falls in the last hypermeasure (the one holding the final measure), else 0;
 *            for an elided cadence 1 when the phrase is a whole number of hypermeasures (the next phrase
 *            then starts on a hyperdownbeat) and 0.5 when it is not; omitted with no arrival
 * False, leaving *out_score untouched, for a length other than 2 or 4, a phrase of no measures or an arrival
 * outside the phrase (past the end, unless elided).
 */
bool standard_meter_hypermeasure_score(unsigned phrase_measures, int arrival_index, bool elided, unsigned length,
                                       double *out_score);

/* LENGTH: the length (2 or 4) with the best score, the longer on a tie, and that score. False as above. */
bool standard_meter_hypermeasure_choose(unsigned phrase_measures, int arrival_index, bool elided, unsigned *out_length,
                                        double *out_score);

/* EXPANSION: the length of a hypermeasure of `base_length` measures that an expansion changes by `added_measures`
   (positive for an expansion, negative for a contraction). False when the result is below 1 or the base is 0. */
bool standard_meter_hypermeasure_extended_length(unsigned base_length, int added_measures, unsigned *out_length);

/* ELISION: the hypermetric counts of the elided measure: the last count of the ending hypermeasure of
   `ending_length` measures, and 1 in the next. False for an ending length of 0. */
bool standard_meter_hypermetric_elision(unsigned ending_length, unsigned *out_count_in_ending, unsigned *out_count_in_next);

#endif

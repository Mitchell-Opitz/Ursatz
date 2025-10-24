#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_RULES_H

#include <stdbool.h>

#include "counterpoint_line.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * First-species harmony rules for standard-counterpoint-v1: what note
 * against note counterpoint asks of the vertical intervals, on top of the
 * rules every species shares (harmonic_rules.h). These are stricter: where
 * the common rules treat direct fifths and octaves as advice, first species
 * forbids them.
 *
 *   ALL_CONSONANT           every interval between the parts is a
 *                           consonance, perfect or imperfect (hard)
 *   NO_REPEATED_PERFECT     no two consecutive intervals are perfect
 *                           consonances of the same size (P5 then P5,
 *                           P8 then P8, unison then unison), whatever the
 *                           motion; compound intervals count as their
 *                           simple counterparts, so P5 then P12 is
 *                           P5 then P5 (hard)
 *   NO_DIRECT_PERFECTS      no arrival at a perfect fifth, octave, or
 *                           unison by similar or parallel motion (hard)
 *   MAX_SAME_IMPERFECT_RUN  no more than three consecutive intervals of
 *                           the same imperfect consonance, three thirds
 *                           in a row at most (hard)
 *   IMPERFECT_PREFERRED     every interval except the first and last is
 *                           an imperfect consonance (soft)
 *   PERFECT_THEN_IMPERFECT  every perfect consonance is followed by an
 *                           imperfect one (soft)
 *   INTERVAL_VARIETY        at least three different simple interval sizes
 *                           appear, or every interval differs when the
 *                           exercise has fewer than three (soft)
 *
 * Intervals are measured between the notes sounding together (lower note
 * to upper note), position by position (counterpoint_line.h). Every check
 * returns false for an invalid pair.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_ALL_CONSONANT = 0,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_REPEATED_PERFECT,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_DIRECT_PERFECTS,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_MAX_SAME_IMPERFECT_RUN,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_IMPERFECT_PREFERRED,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_PERFECT_THEN_IMPERFECT,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT
} StandardCounterpointFirstSpeciesHarmonyRule;

#define STANDARD_COUNTERPOINT_FIRST_SPECIES_MAX_IMPERFECT_RUN 3
#define STANDARD_COUNTERPOINT_FIRST_SPECIES_MIN_INTERVAL_VARIETY 3

const Rule *standard_counterpoint_first_species_harmony_rule(StandardCounterpointFirstSpeciesHarmonyRule which);
const Constraint *
standard_counterpoint_first_species_harmony_constraint(StandardCounterpointFirstSpeciesHarmonyRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_first_species_harmony_preference(StandardCounterpointFirstSpeciesHarmonyRule which);

/* "all-consonant" "no-repeated-perfect" "no-direct-perfects"
   "max-same-imperfect-run" "imperfect-preferred" "perfect-then-imperfect"
   "interval-variety"; NULL for an invalid rule. */
const char *standard_counterpoint_first_species_harmony_rule_name(StandardCounterpointFirstSpeciesHarmonyRule which);

/* True when the two-part texture satisfies the rule. False when it does not,
   and for an invalid pair or an invalid rule. */
bool standard_counterpoint_first_species_harmony_rule_holds(StandardCounterpointFirstSpeciesHarmonyRule which,
                                                            const StandardCounterpointPair *pair);

/* The rules a texture breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False
   (both untouched) for an invalid pair. */
bool standard_counterpoint_first_species_harmony_check(const StandardCounterpointPair *pair,
                                                       unsigned *hard_violations, unsigned *soft_violations);

#endif

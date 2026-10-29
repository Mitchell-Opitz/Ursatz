#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_HARMONIC_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_HARMONIC_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "counterpoint_line.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Harmonic writing rules for standard-counterpoint-v1: the vertical
 * intervals between two parts and how the parts combine. The note at
 * position i of the upper part sounds with the note at position i of the
 * lower part (counterpoint_line.h).
 *
 *   FIRST_INTERVAL         the first interval is a perfect unison, fifth,
 *                          or octave (hard)
 *   LAST_INTERVAL          the last interval is a perfect unison or
 *                          octave, not a fifth (hard)
 *   MAX_GAP                no interval between the parts is larger than a
 *                          twelfth (hard)
 *   MOSTLY_WITHIN_OCTAVE   the gap stays within an octave for the most
 *                          part: no more than a quarter of the intervals
 *                          are larger than an octave (soft)
 *   NO_CROSSING            the upper part never sounds below the lower
 *                          one (voice crossing) (hard)
 *   PARALLEL_OCTAVES       no two consecutive intervals are both perfect
 *                          octaves (or unisons, which are octaves for
 *                          this purpose) reached by parallel motion
 *                          (hard)
 *   PARALLEL_FIFTHS        no two consecutive intervals are both perfect
 *                          fifths reached by parallel motion (hard)
 *   DIRECT_PERFECTS        no arrival at a perfect fifth, octave, or
 *                          unison by similar or parallel motion, from any
 *                          interval ("direct" or "hidden" fifths and
 *                          octaves) (soft)
 *   INTERIOR_UNISONS       no unison anywhere but the first and last
 *                          intervals (soft)
 *   CLIMAX_COORDINATION    the two parts do not sound their highest
 *                          pitches at the same time (soft)
 *
 * Parallel motion is the motion defined in motion_rules.h: same
 * direction, same distance. Compound intervals count as their simple
 * counterparts (a twelfth is a fifth, a fifteenth an octave).
 *
 * Every check returns false for an invalid pair.
 */

typedef enum {
    STANDARD_COUNTERPOINT_HARMONIC_RULE_FIRST_INTERVAL = 0,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_LAST_INTERVAL,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_MAX_GAP,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_INTERIOR_UNISONS,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION,
    STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT
} StandardCounterpointHarmonicRule;

#define STANDARD_COUNTERPOINT_MAX_GAP 12
#define STANDARD_COUNTERPOINT_USUAL_GAP 8

const Rule *standard_counterpoint_harmonic_rule(StandardCounterpointHarmonicRule which);
const Constraint *standard_counterpoint_harmonic_constraint(StandardCounterpointHarmonicRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_harmonic_preference(StandardCounterpointHarmonicRule which);

/* "first-interval" "last-interval" "max-gap" "mostly-within-octave"
   "no-crossing" "parallel-octaves" "parallel-fifths" "direct-perfects"
   "interior-unisons" "climax-coordination"; NULL for an invalid rule. */
const char *standard_counterpoint_harmonic_rule_name(StandardCounterpointHarmonicRule which);

/* True when the two-part texture satisfies the rule. False when it does
   not, and for an invalid pair or an invalid rule. */
bool standard_counterpoint_harmonic_rule_holds(StandardCounterpointHarmonicRule which,
                                               const StandardCounterpointPair *pair);

/* The rules a two-part texture breaks: hard_violations has bit (1u << rule)
   set for each hard rule that fails, soft_violations for each soft rule.
   False (both untouched) for an invalid pair. */
bool standard_counterpoint_harmonic_check(const StandardCounterpointPair *pair, unsigned *hard_violations,
                                          unsigned *soft_violations);

/*
 * A run of equal imperfect consonances between two parts given as MIDI numbers: true when the last
 * `run` vertical intervals (the distance between the parts, mod 12) are all the same imperfect
 * consonance, a third or sixth (3, 4, 8 or 9 semitones). With `run` 3 it is the preference against
 * three of them in a row. This is a preference, not a hard rule (Law 13); it is a plain function and
 * not an entry of the rule table, so the table and every check built on it are unchanged. False for
 * NULL arrays, a `run` of 0, or fewer than `run` intervals.
 */
bool standard_counterpoint_imperfect_run(const int *upper, const int *lower, size_t count, size_t run);

#endif

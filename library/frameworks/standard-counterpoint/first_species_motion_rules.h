#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_RULES_H

#include <stdbool.h>

#include "counterpoint_line.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * First-species motion rules for standard-counterpoint-v1: how the parts
 * move between successive intervals (motion_rules.h), to keep them
 * independent and varied. All four are soft.
 *
 *   MOTION_VARIETY            contrary, similar, and parallel motion each
 *                             appear at least once, once the exercise has
 *                             at least four motions (oblique motion is not
 *                             required)
 *   CONTRARY_PREFERRED        contrary motion is at least as common as each
 *                             of the other types: similar, parallel, and
 *                             oblique
 *   SIMILAR_WITH_LEAP         no similar or parallel motion in which either
 *                             part leaps (an interval of a third or more)
 *   SIMILAR_WITH_LARGE_LEAP   no similar or parallel motion in which either
 *                             part leaps a fifth or more
 *
 * The last is a stricter case of the one before it, kept apart because the
 * advice is strongest for large leaps. The number of motions in an exercise
 * is one less than its number of notes. Every check returns false for an
 * invalid pair.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_VARIETY = 0,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_CONTRARY_PREFERRED,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LEAP,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LARGE_LEAP,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT
} StandardCounterpointFirstSpeciesMotionRule;

#define STANDARD_COUNTERPOINT_FIRST_SPECIES_MIN_MOTIONS_FOR_VARIETY 4
#define STANDARD_COUNTERPOINT_FIRST_SPECIES_LARGE_LEAP 5

const Rule *standard_counterpoint_first_species_motion_rule(StandardCounterpointFirstSpeciesMotionRule which);
const Constraint *
standard_counterpoint_first_species_motion_constraint(StandardCounterpointFirstSpeciesMotionRule which);

/* "motion-variety" "contrary-preferred" "similar-with-leap"
   "similar-with-large-leap"; NULL for an invalid rule. */
const char *standard_counterpoint_first_species_motion_rule_name(StandardCounterpointFirstSpeciesMotionRule which);

/* True when the texture satisfies the rule. False when it does not, and for
   an invalid pair or an invalid rule. */
bool standard_counterpoint_first_species_motion_rule_holds(StandardCounterpointFirstSpeciesMotionRule which,
                                                           const StandardCounterpointPair *pair);

/* The rules the texture breaks, as a bit mask: bit (1u << rule) is set for
   each that fails. All four are soft. False (*violations untouched) for an
   invalid pair. */
bool standard_counterpoint_first_species_motion_check(const StandardCounterpointPair *pair, unsigned *violations);

#endif

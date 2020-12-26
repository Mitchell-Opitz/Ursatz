#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_MODAL_SOLFEGE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_MODAL_SOLFEGE_RULES_H

#include "mode_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Modal solfege rules for standard-tonality-v1. Each mode's degree takes
 * the chromatic syllable (solfege_rules.h) for its alteration against the
 * major scale on the same tonic:
 *
 *   lydian       do re mi fi sol la ti     (raised 4: fi)
 *   ionian       do re mi fa sol la ti
 *   mixolydian   do re mi fa sol la te
 *   dorian       do re me fa sol la te
 *   aeolian      do re me fa sol le te
 *   phrygian     do ra me fa sol le te     (lowered 2: ra)
 *   locrian      do ra me fa se le te      (lowered 2 and 5: ra, se)
 *
 * The bright modes have mi, the dark modes me. Degree 8 is the tonic
 * again, do.
 */

typedef enum {
    STANDARD_TONALITY_MODAL_SOLFEGE_RULE_SYLLABLES = 0,
    STANDARD_TONALITY_MODAL_SOLFEGE_RULE_COUNT
} StandardTonalityModalSolfegeRule;

const Rule *standard_tonality_modal_solfege_rule(StandardTonalityModalSolfegeRule which);
const Constraint *standard_tonality_modal_solfege_constraint(StandardTonalityModalSolfegeRule which);

/* How many semitones degree 1..7 of the mode sits above (+) or below (-)
   the same degree of the major scale: -1, 0, or +1. 0 for an invalid
   mode or degree. */
int standard_tonality_mode_degree_alteration(StandardTonalityMode mode, int degree);

/* SYLLABLES: the syllable of degree 1..8 (8 is do) in the mode. NULL for
   an invalid mode or degree. */
const char *standard_tonality_mode_solfege(StandardTonalityMode mode, int degree);

#endif

#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_RULES_H

#include <stdbool.h>

#include "fifth_species_exercise.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Fifth-species counterpoint line rules for standard-counterpoint-v1: the
 * counterpoint line. The rules read the line as its attacks
 * (fifth_species_exercise.h): the new notes in order, with the rest and the
 * ties left out, so a note held across the bar line counts once. A florid line
 * mixes steps, leaps, and repeated pitches (an anticipation repeats the
 * pitch it anticipates), so the shape rules are those of the cantus firmus
 * (cantus_firmus_rules.h) applied to the attacks.
 *
 *   LEAPS             every melodic leap is a consonant melodic interval: no
 *                     augmented or diminished interval, no seventh, nothing
 *                     larger than an octave, and the only sixth is the
 *                     ascending minor sixth (hard)
 *   FINAL_STEP        the last note is approached by step, from above, or from
 *                     below by a half step (hard); the cantus firmus rule
 *   RANGE             the lowest and highest notes are no more than a tenth
 *                     apart (hard); the cantus firmus rule
 *   LEADING_TONE      a leading tone steps up to the tonic, and a seventh
 *                     degree a whole step below the tonic does not (hard); the
 *                     cantus firmus rule
 *   SINGLE_CLIMAX     the highest note is reached once (soft); the cantus
 *                     firmus rule
 *   LEAP_RECOVERY     a leap is followed by a step in the opposite direction
 *                     (soft); the cantus firmus rule
 *   MOSTLY_STEPWISE   most of the line moves by step (soft); the cantus
 *                     firmus rule
 *   NO_REPEATED_MOTIVE  no group of notes is repeated (soft); the cantus
 *                     firmus rule
 *
 * A leap is an interval of a third or more. Every check needs the tonic and
 * returns false for an invalid exercise or a NULL tonic.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_LEAPS = 0,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_FINAL_STEP,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_RANGE,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_LEADING_TONE,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_SINGLE_CLIMAX,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_LEAP_RECOVERY,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_MOSTLY_STEPWISE,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_NO_REPEATED_MOTIVE,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_COUNT
} StandardCounterpointFifthSpeciesLineRule;

const Rule *standard_counterpoint_fifth_species_line_rule(StandardCounterpointFifthSpeciesLineRule which);
const Constraint *standard_counterpoint_fifth_species_line_constraint(StandardCounterpointFifthSpeciesLineRule which);

/* "leaps" "final-step" "range" "leading-tone" "single-climax" "leap-recovery"
   "mostly-stepwise" "no-repeated-motive"; NULL for an invalid rule. */
const char *standard_counterpoint_fifth_species_line_rule_name(StandardCounterpointFifthSpeciesLineRule which);

/* True when the counterpoint line satisfies the rule. False when it does not,
   and for invalid input. */
bool standard_counterpoint_fifth_species_line_rule_holds(StandardCounterpointFifthSpeciesLineRule which,
                                                         const StandardCounterpointFifthSpeciesExercise *exercise,
                                                         const PitchSpelling *tonic);

/* The rules the line breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for invalid input. */
bool standard_counterpoint_fifth_species_line_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                    const PitchSpelling *tonic, unsigned *hard_violations,
                                                    unsigned *soft_violations);

#endif

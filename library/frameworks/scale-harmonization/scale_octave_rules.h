#ifndef URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_OCTAVE_RULES_H
#define URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_OCTAVE_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "scale_harmonization.h"

/*
 * Rule of the Octave rules for scale-harmonization-v1. The Rule of the Octave is
 * a cheat sheet for harmonizing a bass line: one chord for each scale degree, in
 * an ascending and a descending form. The version here is closely based on
 * Fedele Fenaroli's (Naples, 1775), with four voices throughout, the bass
 * included, and no suggestion of parallels. A harmonization of it is eight
 * stages, one chord on each note of the bass scale from the tonic to the octave
 * (ascending) or from the octave down to the tonic (descending).
 *
 * The chord on each degree (the Rule's own table) is not encoded: the rules
 * are about the structure the chapter states, built up from parallel first
 * inversion chords with 5/3 chords on the first and last.
 *
 * Hard:
 *
 *   FOUR_VOICES      four voices at every stage, the bass included
 *   BASS_IS_SCALE    eight stages whose bass notes are the degrees of a diatonic
 *                    scale in the harmonization's direction (1 2 3 4 5 6 7 1
 *                    ascending, 1 7 6 5 4 3 2 1 descending); the third, sixth,
 *                    and seventh may be lowered (minor)
 *   TONIC_CHORDS     the first and last chords are tonic 5/3 chords: above the
 *                    bass, a third and a fifth, and nothing but a third, a fifth,
 *                    or an octave
 *   NO_PARALLELS     no two voices make parallel fifths or octaves (a unison
 *                    counts as an octave) in consecutive chords
 *
 * Soft:
 *
 *   FIRST_INVERSION  every chord between the first and last is a first
 *                    inversion (6/3) chord: above the bass only a third, a sixth,
 *                    or an octave. The Rule has exceptions (the dominant chords
 *                    and the sevenths), so this is advice
 *
 * Every check returns false for an invalid harmonization.
 */

#define SCALE_OCTAVE_STAGES 8

typedef enum {
    SCALE_OCTAVE_FOUR_VOICES = 0,
    SCALE_OCTAVE_BASS_IS_SCALE,
    SCALE_OCTAVE_TONIC_CHORDS,
    SCALE_OCTAVE_NO_PARALLELS,
    SCALE_OCTAVE_FIRST_INVERSION,
    SCALE_OCTAVE_RULE_COUNT
} ScaleOctaveRule;

const Rule *scale_octave_rule(ScaleOctaveRule which);
const Constraint *scale_octave_constraint(ScaleOctaveRule which);

/* "four-voices" "bass-is-scale" "tonic-chords" "no-parallels" "first-inversion"; NULL
   for an invalid rule. */
const char *scale_octave_rule_name(ScaleOctaveRule which);

/* True when the harmonization satisfies the rule. False when it does not, and
   for an invalid harmonization or an invalid rule. */
bool scale_octave_rule_holds(ScaleOctaveRule which, const ScaleHarmonization *harmonization);

/* The rules the harmonization breaks: hard_violations has bit (1u << rule) set
   for each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid harmonization. */
bool scale_octave_check(const ScaleHarmonization *harmonization, unsigned *hard_violations,
                        unsigned *soft_violations);

#endif

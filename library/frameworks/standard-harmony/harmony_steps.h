#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_STEPS_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_STEPS_H

#include <stdbool.h>

/*
 * Diatonic step positions for standard-harmony-v1. A voice's note is a whole
 * number of steps of the key's scale from a tonic: step 0 is do, 1 is re, 2 is mi,
 * 6 is ti, 7 is do an octave up, -1 is ti an octave down, and so on. So the scale
 * degree of a step is
 *
 *   degree = (step mod 7) + 1        (1 to 7; negative steps wrap upward)
 *
 * and the octave is kept, which a plain degree loses. That is what lets a rule
 * tell the direction a voice moves, and the interval two voices make, to find
 * parallel fifths and octaves. Accidentals are not modeled: the minor mode's
 * altered degrees (me, le, ti) share the degree of their major counterpart.
 *
 * This module carries its own copy of the arithmetic rather than depending on
 * another framework.
 */

typedef enum {
    HARMONY_PERFECT_NONE = 0,
    HARMONY_PERFECT_FIFTH,
    HARMONY_PERFECT_OCTAVE
} HarmonyPerfectInterval;

/* The scale degree (1 to 7) of a step. */
int harmony_step_degree(int step);

/* 1 if the voice moves up (to a higher step), -1 if it moves down, 0 if it
   stays. */
int harmony_step_direction(int from, int to);

/* Whether two voices at these steps make a perfect fifth or an octave (a unison
   counts as an octave; any number of octaves is the same), measured from the
   lower voice up. A fifth between ti and fa (ti below, fa above) is a diminished
   fifth, so it is not perfect. */
HarmonyPerfectInterval harmony_step_perfect_interval(int a, int b);

/* True when two voices move in the same direction, both moving, from one perfect
   interval to the same kind of perfect interval: parallel fifths or octaves. */
bool harmony_steps_parallel(int a_from, int a_to, int b_from, int b_to);

#endif

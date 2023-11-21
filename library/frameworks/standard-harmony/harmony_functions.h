#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_FUNCTIONS_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_FUNCTIONS_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "harmonic_function.h"

/*
 * Harmonic function rules for standard-harmony-v1: which of the three functions
 * a chord has, by the scale degree of its root. A chord's inversion does not
 * change its function.
 *
 *   TONIC         I (minor: i)
 *   PREDOMINANT   strong: IV and ii (minor: iv and ii diminished), which signal
 *                 that a dominant is imminent; weak: iii and vi (minor: VII,
 *                 III, and VI), which transition away from tonic, typically to
 *                 a stronger predominant
 *   DOMINANT      V and vii diminished (the same in minor, where vii is the one
 *                 on the raised, leading-tone seventh degree)
 *
 * The tonic triad in second inversion marked cadential (harmony_chord.h), the
 * cadential 6/4, is a dominant: it embellishes V.
 *
 * In degrees (major): 1 tonic; 2 and 4 strong predominant; 3 and 6 weak
 * predominant; 5 and 7 dominant. In minor, the same, except 7 is a weak
 * predominant (VII) unless the chord's leading_tone is set, when it is a
 * dominant.
 */

typedef enum {
    HARMONY_PREDOMINANT_WEAK = 0,
    HARMONY_PREDOMINANT_STRONG
} HarmonyPredominantStrength;

/* The function of the chord in the mode. False (*out untouched) for an invalid
   mode, an invalid chord, or NULL. */
bool harmony_function_of(HarmonyMode mode, const HarmonyChord *chord, HarmonicFunction *out);

/* Whether the chord is a strong or a weak predominant. False (*out untouched)
   when the chord is not a predominant, and for an invalid mode or chord. */
bool harmony_predominant_strength(HarmonyMode mode, const HarmonyChord *chord, HarmonyPredominantStrength *out);

#endif

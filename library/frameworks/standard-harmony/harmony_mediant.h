#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_MEDIANT_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_MEDIANT_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * The mediant harmonizing mi in the bass, for standard-harmony-v1 (The Mediant Harmonizing
 * Mi in the Bass). The iii chord (III in minor) is a weak predominant, like vi: it usually
 * travels through a strong predominant on its way to V. It is used in one relatively
 * specific situation, after a I chord, harmonizing a descending ti in an upper voice (te in
 * minor), and is usually in root position. It is not a substitute for I6: the bass line do
 * re mi is harmonized I V4/3 I6 (harmony_prolongation.h), never with iii, and V going to iii
 * is not stylistically normative, so it is not a progression here. It is not a very common
 * chord.
 *
 * Three chords: I in root position, then iii (a triad, in root position or, less often, first
 * inversion), then one of
 *
 *   TO_PREDOMINANT  a strong predominant (harmony_strong_predominants.h: IV, ii6, or ii), the
 *                   usual way on to V
 *   TO_DOMINANT     V in root position, directly, the bass arpeggiation do mi sol harmonized
 *                   I iii V
 */

typedef enum {
    HARMONY_MEDIANT_TO_PREDOMINANT = 0,
    HARMONY_MEDIANT_TO_DOMINANT,
    HARMONY_MEDIANT_KIND_COUNT
} HarmonyMediantKind;

/* True for the two kinds above; false for any other value. */
bool harmony_mediant_kind_valid(HarmonyMediantKind kind);

/* "to-predominant" "to-dominant"; NULL for an invalid kind. */
const char *harmony_mediant_kind_name(HarmonyMediantKind kind);

/* The kind the three chords make in the mode. False (*out untouched) for any other three
   chords, for a chord marked cadential, a seventh chord, or an invalid one, an invalid mode,
   and NULL. */
bool harmony_mediant_of(HarmonyMode mode, const HarmonyChord chords[3], HarmonyMediantKind *out);

#endif

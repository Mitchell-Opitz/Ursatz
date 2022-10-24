#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRYGIAN_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRYGIAN_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * The phrygian half cadence (PHC) for standard-harmony-v1 (La in the Bass at Beginnings,
 * Middles, and Endings). A special kind of half cadence that occurs only in minor: the
 * progression iv6 to V, the bass moving le to sol. It is called phrygian because of the
 * half step le makes with sol in the bass, a sound similar to ra moving to do in the
 * phrygian mode. It is still a half cadence (harmony_cadence_rules.h), not a type of its
 * own, so there is no CadenceType for it: this module says whether a progression is one.
 * Very often a PHC is approached from i and accompanied by the soprano me fa sol, which is
 * not modeled (it needs the chords before).
 *
 * The chords are iv6 (the triad on degree 4 in first inversion) and V (the triad on degree
 * 5 in root position, or V7), in a minor key.
 */

/* True when the key is minor and the two chords are iv6, then V or V7 in root position. False
   for a major key, any other pair, a chord marked cadential or an invalid one, an invalid
   mode, and NULL. */
bool harmony_phrygian_half_cadence_is(HarmonyMode mode, const HarmonyChord chords[2]);

#endif

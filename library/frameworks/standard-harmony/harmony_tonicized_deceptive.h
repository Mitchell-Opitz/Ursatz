#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_TONICIZED_DECEPTIVE_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_TONICIZED_DECEPTIVE_H

#include <stdbool.h>

#include "harmony_applied.h"
#include "harmony_chord.h"

/*
 * Tonicized deceptive motion for standard-harmony-v1 (Tonicization). One striking way to use
 * tonicization is to highlight a deceptive motion (harmony_deceptive.h): V moves to vii
 * diminished 7 of vi before the vi chord arrives, which draws our attention even more to the
 * absence of the expected tonic after V. Three chords: V (a triad or a seventh chord, in any
 * inversion), a secondary leading-tone chord whose target is vi (VI in minor), and vi, a triad
 * in root position.
 */

/* True when the three chords are tonicized deceptive motion in the mode. False for any other
   chords, an invalid chord, applied chord, or mode, and for a NULL pointer. */
bool harmony_tonicized_deceptive_is(HarmonyMode mode, const HarmonyChord *dominant,
                                    const HarmonyAppliedChord *applied, const HarmonyChord *resolution);

#endif

#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_DECEPTIVE_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_DECEPTIVE_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * Deceptive motion for standard-harmony-v1 (La in the Bass at Beginnings, Middles, and
 * Endings). When V sets up the expectation of a cadence but moves to an unexpected,
 * non-tonic harmony, that is deceptive motion. It is called motion, not a deceptive
 * cadence: the progression avoids a cadence rather than creating one, so it is never a
 * CadenceType here (a subverted cadence is the caller's flag, harmony_cadence_rules.h).
 * The bass goes sol to la, and la is harmonized by one of two chords:
 *
 *   VI     V or V7 to vi (VI in minor): by far the most common
 *   IV6    V or V7 to IV6: less common
 *
 * The V is in root position, a triad or a seventh chord; the destination is a triad.
 */

typedef enum {
    HARMONY_DECEPTIVE_VI = 0,
    HARMONY_DECEPTIVE_IV6,
    HARMONY_DECEPTIVE_KIND_COUNT
} HarmonyDeceptiveKind;

/* True for the two kinds above; false for any other value. */
bool harmony_deceptive_kind_valid(HarmonyDeceptiveKind kind);

/* "vi" "iv6"; NULL for an invalid kind. */
const char *harmony_deceptive_kind_name(HarmonyDeceptiveKind kind);

/* The kind the two chords make: V (or V7) in root position, then the triad on degree 6
   in root position (VI) or the triad on degree 4 in first inversion (IV6). False (*out
   untouched) for any other pair, for a chord marked cadential or an invalid one, and
   for NULL. */
bool harmony_deceptive_of(const HarmonyChord chords[2], HarmonyDeceptiveKind *out);

#endif

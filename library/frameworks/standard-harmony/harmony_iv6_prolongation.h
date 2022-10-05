#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_IV6_PROLONGATION_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_IV6_PROLONGATION_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * Tonic prolongation with IV6 as a predominant for standard-harmony-v1 (La in the Bass at
 * Beginnings, Middles, and Endings). At a phrase beginning, la in the bass can prolong tonic
 * by way of IV6, a predominant, then a dominant, then tonic again: four chords, the first and
 * last I in root position, the second IV6 (the triad on degree 4 in first inversion), and the
 * third V or V6 (the triad on degree 5 in root position or first inversion). The two bass
 * lines, and the progressions that go with them:
 *
 *   V    do la sol do   I IV6 V I
 *   V6   do la ti do    I IV6 V6 I
 *
 * It does not work well in minor, where the bass makes an augmented second from le to ti.
 */

typedef enum {
    HARMONY_IV6_PROLONGATION_V = 0,
    HARMONY_IV6_PROLONGATION_V6,
    HARMONY_IV6_PROLONGATION_KIND_COUNT
} HarmonyIv6ProlongationKind;

/* True for the two kinds above; false for any other value. */
bool harmony_iv6_prolongation_kind_valid(HarmonyIv6ProlongationKind kind);

/* "v" "v6"; NULL for an invalid kind. */
const char *harmony_iv6_prolongation_kind_name(HarmonyIv6ProlongationKind kind);

/* The kind four chords make: I, IV6, V or V6, I, all triads. False (*out untouched) for any
   other four chords, for a chord marked cadential or an invalid one, and for NULL. */
bool harmony_iv6_prolongation_of(const HarmonyChord chords[4], HarmonyIv6ProlongationKind *out);

#endif

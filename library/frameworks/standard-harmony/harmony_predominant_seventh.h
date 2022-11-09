#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PREDOMINANT_SEVENTH_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PREDOMINANT_SEVENTH_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * Predominant seventh chords for standard-harmony-v1 (Predominant Seventh Chords). Adding a
 * chordal seventh to a predominant chord intensifies its motion to the dominant, as the seventh
 * of V7 strengthens the pull to tonic. It is common with ii, and also occurs, less often, with
 * IV and vi, and rarely with iii. These chords have predominant function, the function of their
 * triads (harmony_functions.h). The kinds:
 *
 *   II7    ii7, root position
 *   II65   ii6/5, first inversion
 *   II43   ii4/3, second inversion
 *   II42   ii4/2, third inversion
 *   IV7    IV7, in any inversion (typically root position)
 *   VI7    vi7, in any inversion (typically root position)
 *   III7   iii7, in any inversion (typically root position)
 *
 * ii7 and its inversions are the most common, and their relative commonality is a value of the
 * kind (HarmonyPredominantSeventhRank), not a rule: ii6/5 most common, ii7 somewhat common,
 * ii4/3 less common, ii4/2 least common. IV7 and vi7 are less common, and iii7, which is not
 * very common as a triad, least. In minor the seventh chord on ii is half diminished and the
 * others have altered notes; accidentals are not modeled.
 */

typedef enum {
    HARMONY_PREDOMINANT_SEVENTH_II7 = 0,
    HARMONY_PREDOMINANT_SEVENTH_II65,
    HARMONY_PREDOMINANT_SEVENTH_II43,
    HARMONY_PREDOMINANT_SEVENTH_II42,
    HARMONY_PREDOMINANT_SEVENTH_IV7,
    HARMONY_PREDOMINANT_SEVENTH_VI7,
    HARMONY_PREDOMINANT_SEVENTH_III7,
    HARMONY_PREDOMINANT_SEVENTH_COUNT
} HarmonyPredominantSeventhKind;

typedef enum {
    HARMONY_PREDOMINANT_SEVENTH_RANK_MOST = 0,
    HARMONY_PREDOMINANT_SEVENTH_RANK_SOMEWHAT,
    HARMONY_PREDOMINANT_SEVENTH_RANK_LESS,
    HARMONY_PREDOMINANT_SEVENTH_RANK_LEAST,
    HARMONY_PREDOMINANT_SEVENTH_RANK_COUNT
} HarmonyPredominantSeventhRank;

/* True for the seven kinds above; false for any other value. */
bool harmony_predominant_seventh_kind_valid(HarmonyPredominantSeventhKind kind);

/* "ii7" "ii6/5" "ii4/3" "ii4/2" "iv7" "vi7" "iii7"; NULL for an invalid kind. */
const char *harmony_predominant_seventh_kind_name(HarmonyPredominantSeventhKind kind);

/* True for the four ranks above; false for any other value. */
bool harmony_predominant_seventh_rank_valid(HarmonyPredominantSeventhRank rank);

/* "most" "somewhat" "less" "least"; NULL for an invalid rank. */
const char *harmony_predominant_seventh_rank_name(HarmonyPredominantSeventhRank rank);

/* How common the kind is, or HARMONY_PREDOMINANT_SEVENTH_RANK_COUNT for an invalid kind. */
HarmonyPredominantSeventhRank harmony_predominant_seventh_rank(HarmonyPredominantSeventhKind kind);

/* The kind of the chord: a seventh chord on degree 2 (by inversion), 4, 6, or 3. False (*out
   untouched) for any other chord, a triad, a chord marked cadential or an invalid one, and
   NULL. */
bool harmony_predominant_seventh_of(const HarmonyChord *chord, HarmonyPredominantSeventhKind *out);

#endif

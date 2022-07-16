#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_STRONG_PREDOMINANTS_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_STRONG_PREDOMINANTS_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * The strong predominants of standard-harmony-v1 (Strengthening Endings with
 * Strong Predominants). Most phrase endings are strengthened by a strong
 * predominant before the V chord, so that a phrase ends with the bass pattern fa
 * to sol in a half cadence or fa to sol to do in an authentic cadence. The fa in
 * the bass is what gets harmonized, and it is harmonized by one of two chords:
 *
 *   IV    the subdominant in root position (the bass doubled almost always, since
 *         it is a root position chord)
 *   ii6   the supertonic in first inversion, the more common of the two (any note
 *         may be doubled, since it is a first inversion chord). In minor, iio6
 *
 * Root position ii is much less common but is a strong predominant in a major
 * key. In minor the diminished triad iio does not normally show up in root
 * position.
 *
 * A chord is one of these only as a triad: a seventh chord, or a chord in any
 * other inversion, is not.
 */

typedef enum {
    HARMONY_STRONG_PREDOMINANT_IV = 0,
    HARMONY_STRONG_PREDOMINANT_II6,
    HARMONY_STRONG_PREDOMINANT_II,
    HARMONY_STRONG_PREDOMINANT_COUNT
} HarmonyStrongPredominant;

/* True for the three kinds above; false for any other value. */
bool harmony_strong_predominant_valid(HarmonyStrongPredominant kind);

/* "iv" "ii6" "ii"; NULL for an invalid kind. */
const char *harmony_strong_predominant_name(HarmonyStrongPredominant kind);

/* The kind of the chord: IV for a root position triad on degree 4, II6 for a
   first inversion triad on degree 2, II for a root position triad on degree 2.
   False (*out untouched) for any other chord, for a seventh chord, and for an
   invalid mode, chord, or NULL. */
bool harmony_strong_predominant_of(HarmonyMode mode, const HarmonyChord *chord, HarmonyStrongPredominant *out);

#endif
